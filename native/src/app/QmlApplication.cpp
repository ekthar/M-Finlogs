#include "app/QmlApplication.h"

#include "app/AppContext.h"
#include "app/QmlBackend.h"
#include "app/SplashScreen.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
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

} // namespace

int runQmlApplication(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("M-Finlogs"));
    app.setOrganizationName(QStringLiteral("Ekthar"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/dashboard.svg")));

    // Qt Quick Controls: use the Basic style so our custom QML theming fully
    // controls the look (Fusion/native styles ignore many custom properties).
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    loadBundledFonts();

    // Branded splash while the database / services warm up.
    SplashScreen splash;
    splash.show();
    splash.runIndeterminate(1400);
    app.processEvents();

    try {
        static AppContext context(QStringLiteral("Ekthar"), QStringLiteral("M-Finlogs"));
        static QmlBackend backend(context);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

        QObject::connect(
            &engine, &QQmlApplicationEngine::objectCreationFailed,
            &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

        engine.loadFromModule(QStringLiteral("MFinlogs"), QStringLiteral("Main"));

        if (engine.rootObjects().isEmpty()) {
            return -1;
        }

        splash.close();
        return app.exec();
    } catch (const std::exception& err) {
        splash.close();
        qCritical("Failed to start M-Finlogs: %s", err.what());
        return 1;
    }
}

} // namespace mfinlogs::app
