#include "data/SqlDatabase.h"
#include "data/ConnectionPool.h"

namespace mfinlogs::data {

SqlDatabase::SqlDatabase(const domain::DatabaseConfig& config)
    : config_(config) {}

SqlDatabase::~SqlDatabase() {
    if (!connectionName_.isEmpty()) {
        ConnectionPool::instance().release(connectionName_);
    }
}

bool SqlDatabase::open() {
    auto& pool = ConnectionPool::instance();
    connectionName_ = pool.acquire(config_);
    return !connectionName_.isEmpty();
}

QSqlDatabase SqlDatabase::handle() const {
    return QSqlDatabase::database(connectionName_);
}

} // namespace mfinlogs::data
