#include "app/ApplicationMode.h"
#include "app/QmlApplication.h"
#include "app/ServerApplication.h"

#include <QCoreApplication>
#include <QStringList>

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
        return mfinlogs::app::runQmlApplication(argc, argv);
    }

    return 1;
}
