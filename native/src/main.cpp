#include "app/ApplicationMode.h"
#include "app/DesktopApplication.h"
#include "app/ServerApplication.h"

#include <QCoreApplication>
#include <QStringList>
// ekthar/m-finlogs/M-Finlogs-C/native/src/main.cpp
#include <QApplication>
#include <QTimer>
#include "app/SplashScreen.h"
#include "app/AuthWindow.h"
#include "app/DesktopApplication.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. Show the modern fade-in splash
    auto *splash = new SplashScreen();
    splash->startFadeIn();

    // 2. Simulate core loading steps securely
    QTimer::singleShot(1000, [=]() { splash->setProgress(30, "Loading runtime DLL context..."); });
    QTimer::singleShot(2000, [=]() { splash->setProgress(65, "Connecting local SQL Server registers..."); });
    QTimer::singleShot(3000, [=]() { splash->setProgress(90, "Applying workspace configuration presets..."); });

    // 3. Chain splash termination to login launch
    QTimer::singleShot(3800, [=]() {
        splash->setProgress(100, "Done.");
        splash->startFadeOut([=]() {
            splash->deleteLater();

            // Launch custom modern Auth screen
            auto *auth = new AuthWindow();
            auth->show();

            // Once user passes login, launch the rest of your system
            QObject::connect(auth, &AuthWindow::authenticated, [=]() {
                auth->deleteLater();
                
                auto *mainWindow = new DesktopApplication();
                mainWindow->show();
            });
        });
    });

    return app.exec();
}

namespace {

QStringList collectArguments(int argc, char** argv) {
    QStringList arguments;
    for (int index = 0; index < argc; index += 1) {
        arguments.push_back(QString::fromLocal8Bit(argv[index]));
    }
    return arguments;
}

mfinlogs::app::ApplicationMode parseMode(const QStringList& arguments) {
    if (arguments.contains(QStringLiteral("--server"))) {
        return mfinlogs::app::ApplicationMode::Server;
    }
    if (arguments.contains(QStringLiteral("--server-install"))) {
        return mfinlogs::app::ApplicationMode::ServerInstall;
    }
    if (arguments.contains(QStringLiteral("--server-uninstall"))) {
        return mfinlogs::app::ApplicationMode::ServerUninstall;
    }
    if (arguments.contains(QStringLiteral("--server-start"))) {
        return mfinlogs::app::ApplicationMode::ServerStart;
    }
    if (arguments.contains(QStringLiteral("--server-stop"))) {
        return mfinlogs::app::ApplicationMode::ServerStop;
    }
    return mfinlogs::app::ApplicationMode::Desktop;
}

} // namespace

int main(int argc, char** argv) {
    const QStringList arguments = collectArguments(argc, argv);
    const mfinlogs::app::ApplicationMode mode = parseMode(arguments);

    switch (mode) {
    case mfinlogs::app::ApplicationMode::Server:
        return mfinlogs::app::runServerApplication(argc, argv);
    case mfinlogs::app::ApplicationMode::ServerInstall:
    {
        QCoreApplication qtApp(argc, argv);
        return mfinlogs::app::installServerMode();
    }
    case mfinlogs::app::ApplicationMode::ServerUninstall:
    {
        QCoreApplication qtApp(argc, argv);
        return mfinlogs::app::uninstallServerMode();
    }
    case mfinlogs::app::ApplicationMode::ServerStart:
    {
        QCoreApplication qtApp(argc, argv);
        return mfinlogs::app::startServerMode();
    }
    case mfinlogs::app::ApplicationMode::ServerStop:
    {
        QCoreApplication qtApp(argc, argv);
        return mfinlogs::app::stopServerMode();
    }
    case mfinlogs::app::ApplicationMode::Desktop:
        return mfinlogs::app::runDesktopApplication(argc, argv);
    }

    return 1;
}
