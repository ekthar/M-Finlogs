#include "data/SqlDatabase.h"

#include <QStringList>
#include <QUuid>

namespace mfinlogs::data {
namespace {

QString odbcValue(const QString& value) {
    if (!value.contains(QLatin1Char(';')) && !value.contains(QLatin1Char('{')) && !value.contains(QLatin1Char('}'))) {
        return value;
    }
    return QStringLiteral("{%1}").arg(QString(value).replace(QStringLiteral("}"), QStringLiteral("}}")));
}

} // namespace

SqlDatabase::SqlDatabase(const domain::DatabaseConfig& config)
    : connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      config_(config) {}

SqlDatabase::~SqlDatabase() {
    if (QSqlDatabase::contains(connectionName_)) {
        QSqlDatabase database = QSqlDatabase::database(connectionName_);
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool SqlDatabase::open() {
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QODBC"), connectionName_);
    database.setDatabaseName(connectionString());
    database.setConnectOptions(QStringLiteral("SQL_ATTR_LOGIN_TIMEOUT=5;SQL_ATTR_CONNECTION_TIMEOUT=10"));
    return database.open();
}

QSqlDatabase SqlDatabase::handle() const {
    return QSqlDatabase::database(connectionName_);
}

QString SqlDatabase::connectionString() const {
    QStringList parts;
    parts << QStringLiteral("DRIVER=%1").arg(config_.driver);
    parts << QStringLiteral("SERVER=%1").arg(odbcValue(config_.server.trimmed()));
    parts << QStringLiteral("DATABASE=%1").arg(odbcValue(config_.database.trimmed()));
    parts << QStringLiteral("TrustServerCertificate=yes");

    if (config_.useWindowsAuth) {
        parts << QStringLiteral("Trusted_Connection=yes");
    } else {
        parts << QStringLiteral("UID=%1").arg(odbcValue(config_.username.trimmed()));
        parts << QStringLiteral("PWD=%1").arg(odbcValue(config_.password));
    }

    return parts.join(QStringLiteral(";")) + QStringLiteral(";");
}

} // namespace mfinlogs::data
