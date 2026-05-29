#include "data/ConfigService.h"

#include "data/SqlDatabase.h"
#include "domain/DomainErrors.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSqlError>

namespace mfinlogs::data {

JsonConfigService::JsonConfigService(const QString& appDataDir)
    : appDataDir_(appDataDir) {}

QString JsonConfigService::configPath() const {
    return QDir(appDataDir_).filePath(QStringLiteral("db_config.json"));
}

QString packagedConfigPath() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("db_config.json"));
}

QString workingDirectoryConfigPath() {
    return QDir(QDir::currentPath()).filePath(QStringLiteral("db_config.json"));
}

QString readableConfigPath(const QString& appDataPath) {
    if (QFile::exists(appDataPath)) {
        return appDataPath;
    }
    const QString packagedPath = packagedConfigPath();
    if (QFile::exists(packagedPath)) {
        return packagedPath;
    }
    const QString workingPath = workingDirectoryConfigPath();
    if (QFile::exists(workingPath)) {
        return workingPath;
    }
    return appDataPath;
}

domain::DatabaseConfig JsonConfigService::readDatabaseConfig() const {
    const QString path = readableConfigPath(configPath());
    QFile file(path);
    if (!file.exists()) {
        return domain::DatabaseConfig{
            QStringLiteral("localhost"),
            QStringLiteral("Finlogs"),
            QString(),
            QString(),
            QStringLiteral("{ODBC Driver 17 for SQL Server}"),
            QStringLiteral("http://127.0.0.1:8000"),
            QStringLiteral("D:/finlogs"),
            true
        };
    }

    if (!file.open(QIODevice::ReadOnly)) {
        throw domain::DomainError(QStringLiteral("Could not read database config at %1: %2").arg(path, file.errorString()).toStdString());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString detail = parseError.error == QJsonParseError::NoError
            ? QStringLiteral("expected a JSON object")
            : parseError.errorString();
        throw domain::DomainError(QStringLiteral("Invalid database config JSON at %1: %2")
            .arg(path, detail)
            .toStdString());
    }

    const QJsonObject payload = document.object();
    const QString authType = payload.value(QStringLiteral("auth_type")).toString();
    const bool useWindowsAuth = authType.isEmpty()
        ? payload.value(QStringLiteral("windows_auth")).toBool(true)
        : authType != QStringLiteral("sql");

    return domain::DatabaseConfig{
        payload.value(QStringLiteral("server")).toString(QStringLiteral("localhost")),
        payload.value(QStringLiteral("database")).toString(QStringLiteral("Finlogs")),
        payload.value(QStringLiteral("username")).toString(),
        payload.value(QStringLiteral("password")).toString(),
        payload.value(QStringLiteral("driver")).toString(QStringLiteral("{ODBC Driver 17 for SQL Server}")),
        payload.value(QStringLiteral("api_base")).toString(QStringLiteral("http://127.0.0.1:8000")),
        payload.value(QStringLiteral("backup_dir")).toString(QStringLiteral("D:/finlogs")),
        useWindowsAuth
    };
}

void JsonConfigService::writeDatabaseConfig(const domain::DatabaseConfig& config) {
    QDir directory(appDataDir_);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        throw domain::DomainError(QStringLiteral("Could not create config directory: %1").arg(appDataDir_).toStdString());
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("server"), config.server);
    payload.insert(QStringLiteral("database"), config.database);
    payload.insert(QStringLiteral("username"), config.username);
    payload.insert(QStringLiteral("password"), config.password);
    payload.insert(QStringLiteral("driver"), config.driver);
    payload.insert(QStringLiteral("api_base"), config.apiBaseUrl);
    payload.insert(QStringLiteral("backup_dir"), config.backupDir);
    payload.insert(QStringLiteral("windows_auth"), config.useWindowsAuth);
    payload.insert(QStringLiteral("auth_type"), config.useWindowsAuth ? QStringLiteral("windows") : QStringLiteral("sql"));

    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw domain::DomainError(QStringLiteral("Could not write database config: %1").arg(file.errorString()).toStdString());
    }
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        throw domain::DomainError(QStringLiteral("Could not commit database config: %1").arg(file.errorString()).toStdString());
    }
}

bool JsonConfigService::testDatabaseConfig(const domain::DatabaseConfig& config) {
    if (config.server.trimmed().isEmpty()) {
        throw domain::DomainError("SQL Server instance is required");
    }
    if (config.database.trimmed().isEmpty()) {
        throw domain::DomainError("SQL database name is required");
    }
    if (!config.useWindowsAuth && config.username.trimmed().isEmpty()) {
        throw domain::DomainError("SQL username is required when SQL Server Authentication is selected");
    }

    SqlDatabase database(config);
    if (database.open()) {
        return true;
    }
    const QString detail = database.handle().lastError().text();
    throw domain::DomainError(QStringLiteral("SQL connection failed. server=%1 database=%2 auth=%3 error=%4")
        .arg(config.server, config.database, config.useWindowsAuth ? QStringLiteral("windows") : QStringLiteral("sql"), detail)
        .toStdString());
}

} // namespace mfinlogs::data
