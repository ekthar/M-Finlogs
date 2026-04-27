#pragma once

#include <QSqlDatabase>
#include <QString>

namespace mfinlogs::data {

class SchemaInitializer final {
public:
    explicit SchemaInitializer(QSqlDatabase database);

    void initialize(const QString& companyName);

private:
    void executeStatement(const QString& sql);
    void ensureCoreTables();
    void ensureMigrations(const QString& companyName);
    void ensureIndexes();
    void ensureDefaultSettings(const QString& companyName);

    QSqlDatabase database_;
};

} // namespace mfinlogs::data

