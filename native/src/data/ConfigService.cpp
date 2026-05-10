#include "data/ConfigService.h"

#include "data/SqlDatabase.h"
#include "domain/DomainErrors.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace mfinlogs::data {

JsonConfigService::JsonConfigService(const QString& appDataDir)
    : appDataDir_(appDataDir) {}

QString JsonConfigService::configPath() const {
    return QDir(appDataDir_).filePath(QStringLiteral("db_config.json"));
}

domain::DatabaseConfig JsonConfigService::readDatabaseConfig() const {
    QFile file(configPath());
    if (!file.exists()) {
        return domain::DatabaseConfig{
            QStringLiteral("localhost"),
            QStringLiteral("finlogs"),
            QString(),
            QString(),
            QStringLiteral("{ODBC Driver 17 for SQL Server}"),
            QStringLiteral("http://127.0.0.1:8000"),
            QStringLiteral("D:/finlogs"),
            true
        };
    }

    if (!file.open(QIODevice::ReadOnly)) {
        throw domain::DomainError(QStringLiteral("Could not read database config: %1").arg(file.errorString()).toStdString());
    }

    const QJsonObject payload = QJsonDocument::fromJson(file.readAll()).object();
    const QString authType = payload.value(QStringLiteral("auth_type")).toString();
    const bool useWindowsAuth = authType.isEmpty()
        ? payload.value(QStringLiteral("windows_auth")).toBool(true)
        : authType != QStringLiteral("sql");

    return domain::DatabaseConfig{
        payload.value(QStringLiteral("server")).toString(QStringLiteral("localhost")),
        payload.value(QStringLiteral("database")).toString(QStringLiteral("finlogs")),
        payload.value(QStringLiteral("username")).toString(),
        payload.value(QStringLiteral("password")).toString(),
        payload.value(QStringLiteral("driver")).toString(QStringLiteral("{ODBC Driver 17 for SQL Server}")),
        payload.value(QStringLiteral("api_base")).toString(QStringLiteral("http://127.0.0.1:8000")),
        payload.value(QStringLiteral("backup_dir")).toString(QStringLiteral("D:/finlogs")),
        useWindowsAuth
    };
}

void JsonConfigService::writeDatabaseConfig(const domain::DatabaseConfig& config) {
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

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw domain::DomainError(QStringLiteral("Could not write database config: %1").arg(file.errorString()).toStdString());
    }
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
}

bool JsonConfigService::testDatabaseConfig(const domain::DatabaseConfig& config) {
    SqlDatabase database(config);
    return database.open();
}

} // namespace mfinlogs::data
