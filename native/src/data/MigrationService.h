#pragma once

#include "domain/Types.h"

#include <QString>

namespace mfinlogs::data {

// Migrates all data from a SQL Server database to a local SQLite file.
// Returns the path to the created SQLite database.
// Throws domain::DomainError on failure.
QString migrateServerToSqlite(const domain::DatabaseConfig& serverConfig, const QString& sqlitePath);

} // namespace mfinlogs::data
