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

private:
    domain::DatabaseConfig config_;
    QString connectionName_;
};

} // namespace mfinlogs::data
