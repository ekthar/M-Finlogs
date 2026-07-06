#pragma once

#include "domain/Types.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QString>
#include <QHash>
#include <QSet>

namespace mfinlogs::data {

class ConnectionPool {
public:
    static ConnectionPool& instance();

    QString acquire(const domain::DatabaseConfig& config);
    void release(const QString& connectionName);

    void markInitialized(const domain::DatabaseConfig& config);
    bool isInitialized(const domain::DatabaseConfig& config) const;

    void closeAll();

private:
    ConnectionPool() = default;
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    static QString buildConnectionString(const domain::DatabaseConfig& config);
    static QString configFingerprint(const domain::DatabaseConfig& config);
    bool healthCheck(const QString& connectionName);

    mutable QMutex mutex_;
    QHash<QString, QString> cache_;
    QHash<QString, int> refcounts_;
    QSet<QString> initialized_;
};

} // namespace mfinlogs::data
