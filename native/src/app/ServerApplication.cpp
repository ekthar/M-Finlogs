#include "app/ServerApplication.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace {

QString serverPidPath() {
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir directory(appDataDir);
    if (!directory.exists()) {
        directory.mkpath(QStringLiteral("."));
    }
    return directory.filePath(QStringLiteral("server.pid"));
}

void writeServerPid() {
    QFile file(serverPidPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QByteArray::number(QCoreApplication::applicationPid()));
    }
}

} // namespace

namespace mfinlogs::app {

ServerApplication::ServerApplication(AppContext& context)
    : context_(context) {
    registerRoutes();
}

void ServerApplication::registerRoutes() {
    server_.route("/", []() {
        return QStringLiteral("M-Finlogs native server");
    });

    server_.route("/health", []() {
        QJsonObject payload;
        payload.insert(QStringLiteral("status"), QStringLiteral("ok"));
        payload.insert(QStringLiteral("runtime"), QStringLiteral("native"));
        return QJsonDocument(payload).toJson(QJsonDocument::Compact);
    });

    server_.route("/companies", [this]() {
        try {
            return QJsonDocument(context_.services().companies->listCompanies()).toJson(QJsonDocument::Compact);
        } catch (const std::exception& err) {
            QJsonObject payload;
            payload.insert(QStringLiteral("detail"), QString::fromUtf8(err.what()));
            return QJsonDocument(payload).toJson(QJsonDocument::Compact);
        }
    });
}

int ServerApplication::run(quint16 port) {
    const quint16 boundPort = server_.listen(QHostAddress::Any, port);
    if (boundPort == 0) {
        qCritical("Could not start native server");
        return 1;
    }
    writeServerPid();
    return QCoreApplication::exec();
}

int runServerApplication(int argc, char** argv) {
    QCoreApplication qtApp(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("M-Finlogs"));
    QCoreApplication::setApplicationName(QStringLiteral("M-Finlogs"));

    AppContext context(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
    ServerApplication server(context);
    return server.run(8000);
}

int installServerMode() {
    const QString command = QStringLiteral("schtasks");
    const QString taskRunCommand = QStringLiteral("\"%1\" --server").arg(QCoreApplication::applicationFilePath());
    const QStringList arguments = {
        QStringLiteral("/Create"),
        QStringLiteral("/F"),
        QStringLiteral("/SC"),
        QStringLiteral("ONLOGON"),
        QStringLiteral("/RL"),
        QStringLiteral("LIMITED"),
        QStringLiteral("/TN"),
        QStringLiteral("M-FinlogsServer"),
        QStringLiteral("/TR"),
        taskRunCommand
    };
    return QProcess::execute(command, arguments);
}

int uninstallServerMode() {
    const QString command = QStringLiteral("schtasks");
    const QStringList arguments = {
        QStringLiteral("/Delete"),
        QStringLiteral("/F"),
        QStringLiteral("/TN"),
        QStringLiteral("M-FinlogsServer")
    };
    return QProcess::execute(command, arguments);
}

int startServerMode() {
    return QProcess::startDetached(QCoreApplication::applicationFilePath(), { QStringLiteral("--server") }) ? 0 : 1;
}

int stopServerMode() {
    QFile file(serverPidPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QString pid = QString::fromUtf8(file.readAll()).trimmed();
    if (pid.isEmpty()) {
        QFile::remove(serverPidPath());
        return 0;
    }

    const QString command = QStringLiteral("taskkill");
    const QStringList arguments = {
        QStringLiteral("/F"),
        QStringLiteral("/PID"),
        pid
    };
    const int exitCode = QProcess::execute(command, arguments);
    QFile::remove(serverPidPath());
    return exitCode;
}

} // namespace mfinlogs::app
