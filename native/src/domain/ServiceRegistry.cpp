#include "domain/ServiceRegistry.h"

#include "data/ConfigService.h"
#include "data/SqlBusinessServices.h"
#include "data/SqliteBusinessServices.h"

#include <QDir>
#include <memory>

namespace mfinlogs::domain {

ServiceRegistry createServiceRegistry(const QString& appDataDir) {
    const std::shared_ptr<ConfigService> config = std::make_shared<data::JsonConfigService>(appDataDir);

    // Read the database config to determine mode (Server or Local/SQLite)
    try {
        const DatabaseConfig dbConfig = config->readDatabaseConfig();
        if (dbConfig.mode == DatabaseMode::Local) {
            // Local mode: use embedded SQLite
            QString sqlitePath = dbConfig.sqlitePath;
            if (sqlitePath.trimmed().isEmpty()) {
                // Default SQLite path: appDataDir/finlogs.db
                sqlitePath = QDir(appDataDir).filePath(QStringLiteral("finlogs.db"));
            }
            return data::createSqliteBusinessServices(sqlitePath, config);
        }
    } catch (...) {
        // If config read fails, fall through to SQL Server (default)
    }

    // Server mode (default): use SQL Server via ODBC
    return data::createSqlBusinessServices(config);
}

} // namespace mfinlogs::domain
