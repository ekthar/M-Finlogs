#include "data/SqlDatabase.h"

#include <QStringList>
#include <QUuid>

namespace mfinlogs::data {

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
    return database.open();
}

QSqlDatabase SqlDatabase::handle() const {
    return QSqlDatabase::database(connectionName_);
}

QString SqlDatabase::connectionString() const {
    QStringList parts;
    parts << QStringLiteral("DRIVER=%1").arg(config_.driver);
    parts << QStringLiteral("SERVER=%1").arg(config_.server);
    parts << QStringLiteral("DATABASE=%1").arg(config_.database);
    parts << QStringLiteral("TrustServerCertificate=yes");

    if (config_.useWindowsAuth) {
        parts << QStringLiteral("Trusted_Connection=yes");
    } else {
        parts << QStringLiteral("UID=%1").arg(config_.username);
        parts << QStringLiteral("PWD=%1").arg(config_.password);
    }

    return parts.join(QStringLiteral(";")) + QStringLiteral(";");
}

} // namespace mfinlogs::data
