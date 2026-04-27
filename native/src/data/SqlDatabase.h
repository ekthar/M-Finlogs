#pragma once

#include "domain/Types.h"

#include <QSqlDatabase>
#include <QString>

namespace mfinlogs::data {

class SqlDatabase final {
public:
    explicit SqlDatabase(const domain::DatabaseConfig& config);
    ~SqlDatabase();

    bool open();
    QSqlDatabase handle() const;
    QString connectionString() const;

private:
    QString connectionName_;
    domain::DatabaseConfig config_;
};

} // namespace mfinlogs::data
