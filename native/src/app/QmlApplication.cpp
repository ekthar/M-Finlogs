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

QString writeStartupDiagnostic(const QString& detail) {
    const bool verificationRun = QCoreApplication::arguments().contains(QStringLiteral("--verify-qml"));
    const QString dataDir = verificationRun
        ? QCoreApplication::applicationDirPath()
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString logPath = QDir(dataDir.isEmpty() ? QDir::homePath() : dataDir)
        .filePath(QStringLiteral("startup-error.log"));
    QFile log(logPath);
    if (log.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&log);
        stream << detail << Qt::endl;
    }
    return logPath;
}

void reportStartupFailure(const QString& detail) {
    const QString logPath = writeStartupDiagnostic(detail);
    QMessageBox::critical(nullptr,
        QStringLiteral("M-Finlogs could not start"),
        QStringLiteral("The application could not load its interface.\n\n%1\n\n"
                       "A diagnostic was saved to:\n%2")
            .arg(detail, QDir::toNativeSeparators(logPath)));
}

} // namespace

int runQmlApplication(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("M-Finlogs"));
    app.setOrganizationName(QStringLiteral("Ekthar"));
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

    // Branded splash — show early and keep visible until QML window renders.
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

    try {
        static AppContext context(QStringLiteral("Ekthar"), QStringLiteral("M-Finlogs"));
        static QmlBackend backend(context);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

        QStringList qmlErrors;
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
            [&qmlErrors](const QList<QQmlError>& warnings) {
                for (const QQmlError& warning : warnings) {
                    qmlErrors.append(warning.toString());
                }
            });

        engine.loadFromModule(QStringLiteral("MFinlogs"), QStringLiteral("Main"));

        if (engine.rootObjects().isEmpty()) {
            const QString detail = qmlErrors.isEmpty()
                ? QStringLiteral("The MFinlogs QML module did not create a root window.")
                : qmlErrors.join(QLatin1Char('\n'));
            qCritical().noquote() << "M-Finlogs QML startup failure:" << detail;
            writeStartupDiagnostic(detail);
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail);
            }
            return -1;
        }

        auto* mainWindow = qobject_cast<QWindow*>(engine.rootObjects().constFirst());
        if (!mainWindow) {
            const QString detail = QStringLiteral("The QML root object is not a window.");
            if (!verifyQml) {
                splash.close();
                reportStartupFailure(detail);
            }
            return -1;
        }
        mainWindow->setVisible(true);
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();

        if (verifyQml) {
            app.processEvents();
            return 0;
        }

        // Keep the animated splash visible long enough to enjoy it, then
        // cross-fade out while the QML window is already rendered behind it.
        QTimer::singleShot(2000, &splash, [&splash, &app]() {
            splash.close();
            app.setQuitOnLastWindowClosed(true);
        });

        return app.exec();
    } catch (const std::exception& err) {
        splash.close();
        qCritical("Failed to start M-Finlogs: %s", err.what());
        if (!verifyQml) {
            reportStartupFailure(QString::fromUtf8(err.what()));
        }
        return 1;
    }
}

} // namespace mfinlogs::app
