#include "app/QmlApplication.h"

#include "app/AppContext.h"
#include "app/QmlBackend.h"
#include "app/SplashScreen.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <QtGlobal>

#include <exception>

namespace mfinlogs::app {

namespace {

// ─── Startup stage tracking ─────────────────────────────────────────────────

enum class StartupStage {
    NotStarted,
    QApplicationCreated,
    SplashShown,
    BackendCreated,
    QmlEngineCreated,
    QmlModuleLoaded,
    RootWindowCreated,
    RootWindowVisible,
    EventLoopStarted,
};

const char* startupStageName(StartupStage stage) {
    switch (stage) {
        case StartupStage::NotStarted:          return "NotStarted";
        case StartupStage::QApplicationCreated: return "QApplicationCreated";
        case StartupStage::SplashShown:         return "SplashShown";
        case StartupStage::BackendCreated:      return "BackendCreated";
        case StartupStage::QmlEngineCreated:    return "QmlEngineCreated";
        case StartupStage::QmlModuleLoaded:     return "QmlModuleLoaded";
        case StartupStage::RootWindowCreated:   return "RootWindowCreated";
        case StartupStage::RootWindowVisible:   return "RootWindowVisible";
        case StartupStage::EventLoopStarted:    return "EventLoopStarted";
    }
    return "Unknown";
}

// Global stage tracker - accessible from terminate handler.
static StartupStage g_currentStage = StartupStage::NotStarted;
static QStringList g_qmlErrors;

// ─── Font loading ───────────────────────────────────────────────────────────

void loadBundledFonts() {
    // Inter Tight + Space Mono ship in a sibling fonts/ directory. Mirror the
    // multi-path search used by the legacy widgets app so packaging is robust.
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList fontSearchPaths = {
        appDir + QStringLiteral("/fonts/"),
        appDir + QStringLiteral("/../fonts/"),
        appDir + QStringLiteral("/../../fonts/"),
        appDir + QStringLiteral("/../../../fonts/"),
    };
    for (const QString& fontDir : fontSearchPaths) {
        if (QDir(fontDir).exists()) {
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Bold.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Bold.ttf"));
            break;
        }
    }
}

// ─── Diagnostic file writer ─────────────────────────────────────────────────

QString writeStartupDiagnostic(const QString& detail,
                               const QQmlApplicationEngine* engine = nullptr) {
    const bool verificationRun = QCoreApplication::arguments().contains(QStringLiteral("--verify-qml"));
    const QString dataDir = verificationRun
        ? QCoreApplication::applicationDirPath()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logPath = QDir(dataDir.isEmpty() ? QDir::homePath() : dataDir)
        .filePath(QStringLiteral("startup-error.log"));

    QDir().mkpath(dataDir.isEmpty() ? QDir::homePath() : dataDir);

    QFile log(logPath);
    if (log.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&log);

        stream << QStringLiteral("=== M-Finlogs Startup Diagnostic ===") << Qt::endl;
        stream << Qt::endl;

        // Current startup stage
        stream << QStringLiteral("Startup Stage: ")
               << QString::fromLatin1(startupStageName(g_currentStage)) << Qt::endl;
        stream << Qt::endl;

        // Executable path
        stream << QStringLiteral("Executable Path: ")
               << QCoreApplication::applicationFilePath() << Qt::endl;

        // Application data path
        stream << QStringLiteral("App Data Path: ")
               << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) << Qt::endl;

        // Platform
        stream << QStringLiteral("Platform: ")
               << QGuiApplication::platformName() << Qt::endl;
        stream << Qt::endl;

        // Qt library paths
        stream << QStringLiteral("Qt Library Paths:") << Qt::endl;
        const QStringList libPaths = QCoreApplication::libraryPaths();
        for (const QString& path : libPaths) {
            stream << QStringLiteral("  - ") << path << Qt::endl;
        }
        stream << Qt::endl;

        // QML import paths (if engine is available)
        if (engine) {
            stream << QStringLiteral("QML Import Paths:") << Qt::endl;
            const QStringList importPaths = engine->importPathList();
            for (const QString& path : importPaths) {
                stream << QStringLiteral("  - ") << path << Qt::endl;
            }
            stream << Qt::endl;
        }

        // QML module path (embedded in binary at this resource prefix)
        stream << QStringLiteral("QML Module Path: :/qt/qml/MFinlogs") << Qt::endl;
        stream << Qt::endl;

        // Database mode
        stream << QStringLiteral("Database Mode: SQLite (local)") << Qt::endl;
        stream << Qt::endl;

        // QML errors collected during loading
        if (!g_qmlErrors.isEmpty()) {
            stream << QStringLiteral("QML Errors:") << Qt::endl;
            for (const QString& err : g_qmlErrors) {
                stream << QStringLiteral("  ") << err << Qt::endl;
            }
            stream << Qt::endl;
        }

        // Exception/failure detail
        stream << QStringLiteral("Failure Detail:") << Qt::endl;
        stream << detail << Qt::endl;
    }
    return logPath;
}

// ─── Startup failure reporter ───────────────────────────────────────────────

void reportStartupFailure(const QString& detail,
                          const QQmlApplicationEngine* engine = nullptr) {
    const QString logPath = writeStartupDiagnostic(detail, engine);
    QMessageBox::critical(nullptr,
        QStringLiteral("M-Finlogs could not start"),
        QStringLiteral("The application could not load its interface.\n\n%1\n\n"
                       "A diagnostic was saved to:\n%2")
            .arg(detail, QDir::toNativeSeparators(logPath)));
}

// ─── Terminate handler ──────────────────────────────────────────────────────

void startupTerminateHandler() {
    // After std::terminate the heap may be corrupt and locks may be held.
    // Keep this handler minimal: attempt a file write wrapped in try/catch,
    // then abort immediately. No QMessageBox (unsafe - requires event loop
    // and window creation in a potentially corrupt state).
    try {
        const QString detail = QStringLiteral(
            "std::terminate called during startup (possible uncaught exception or abort).");
        writeStartupDiagnostic(detail);
    } catch (...) {
        // Nothing we can safely do here.
    }
    std::abort();
}

} // namespace

int runQmlApplication(int argc, char** argv) {
    // Install terminate handler early to catch unexpected terminations.
    std::set_terminate(startupTerminateHandler);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("M-Finlogs"));
    app.setOrganizationName(QStringLiteral("Ekthar"));
    g_currentStage = StartupStage::QApplicationCreated;

    // The QWidget splash and the QML ApplicationWindow are separate top-level
    // windows. Keep the process alive while ownership moves from one to the
    // other; otherwise closing the splash can trigger quitOnLastWindowClosed.
    app.setQuitOnLastWindowClosed(false);

    // Set application icon (shows in taskbar + window chrome)
    const QIcon appIcon(QStringLiteral(":/icons/logo.svg"));
    app.setWindowIcon(appIcon);

    // Qt Quick Controls: use the Basic style so our custom QML theming fully
    // controls the look (Fusion/native styles ignore many custom properties).
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    loadBundledFonts();

    // Branded splash -- show early and keep visible until QML window renders.
    const bool verifyQml = QCoreApplication::arguments().contains(QStringLiteral("--verify-qml"));
    SplashScreen splash;
    if (!verifyQml) {
        splash.show();
        splash.runIndeterminate(2200);
        app.processEvents();
        // Process events several times to ensure splash paint completes.
        for (int i = 0; i < 5; ++i) {
            QThread::msleep(50);
            app.processEvents();
        }
    }
    g_currentStage = StartupStage::SplashShown;

    try {
        static AppContext context(QStringLiteral("Ekthar"), QStringLiteral("M-Finlogs"));
        static QmlBackend backend(context);
        g_currentStage = StartupStage::BackendCreated;

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        g_currentStage = StartupStage::QmlEngineCreated;

        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
            [](const QList<QQmlError>& warnings) {
                for (const QQmlError& warning : warnings) {
                    g_qmlErrors.append(warning.toString());
                }
            });

        engine.loadFromModule(QStringLiteral("MFinlogs"), QStringLiteral("Main"));
        g_currentStage = StartupStage::QmlModuleLoaded;

        if (engine.rootObjects().isEmpty()) {
            const QString detail = g_qmlErrors.isEmpty()
                ? QStringLiteral("The MFinlogs QML module did not create a root window.")
                : g_qmlErrors.join(QLatin1Char('\n'));
            qCritical().noquote() << "M-Finlogs QML startup failure:" << detail;
            writeStartupDiagnostic(detail, &engine);
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail, &engine);
            }
            return -1;
        }

        auto* mainWindow = qobject_cast<QWindow*>(engine.rootObjects().constFirst());
        if (!mainWindow) {
            const QString detail = QStringLiteral(
                "The QML root object is not a QWindow/ApplicationWindow. "
                "Root object type: %1")
                .arg(QString::fromLatin1(engine.rootObjects().constFirst()->metaObject()->className()));
            qCritical().noquote() << "M-Finlogs QML startup failure:" << detail;
            writeStartupDiagnostic(detail, &engine);
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail, &engine);
            }
            return -1;
        }
        g_currentStage = StartupStage::RootWindowCreated;

        mainWindow->setVisible(true);
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();
        g_currentStage = StartupStage::RootWindowVisible;

        if (verifyQml) {
            app.processEvents();
            if (!g_qmlErrors.isEmpty()) {
                writeStartupDiagnostic(g_qmlErrors.join(QLatin1Char('\n')), &engine);
                return 2;
            }
            return 0;
        }

        // Keep the animated splash visible long enough to enjoy it, then
        // cross-fade out while the QML window is already rendered behind it.
        QTimer::singleShot(2000, &splash, [&splash, &app]() {
            splash.close();
            app.setQuitOnLastWindowClosed(true);
        });

        g_currentStage = StartupStage::EventLoopStarted;
        return app.exec();
    } catch (const std::exception& err) {
        splash.close();
        const QString detail = QString::fromUtf8(err.what());
        qCritical("Failed to start M-Finlogs: %s", err.what());
        if (!verifyQml) {
            reportStartupFailure(detail);
        } else {
            writeStartupDiagnostic(detail);
        }
        return 1;
    } catch (...) {
        splash.close();
        const QString detail = QStringLiteral("Unknown exception during startup.");
        qCritical("Failed to start M-Finlogs: unknown exception");
        if (!verifyQml) {
            reportStartupFailure(detail);
        } else {
            writeStartupDiagnostic(detail);
        }
        return 1;
    }
}

} // namespace mfinlogs::app
