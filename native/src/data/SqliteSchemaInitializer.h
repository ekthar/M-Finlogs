#pragma once

#include <QSqlDatabase>
#include <QString>

namespace mfinlogs::data {

// SQLite equivalent of SchemaInitializer. Creates the same logical schema
// using SQLite-compatible DDL (AUTOINCREMENT, TEXT, REAL, datetime('now')).
class SqliteSchemaInitializer final {
public:
    explicit SqliteSchemaInitializer(QSqlDatabase database);

    void initialize(const QString& companyName);

private:
    void executeStatement(const QString& sql);
    void ensureCoreTables();
    void ensureIndexes();
    void ensureDefaultSettings(const QString& companyName);

    QSqlDatabase database_;
};

} // namespace mfinlogs::data
