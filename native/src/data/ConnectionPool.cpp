#include "data/ConnectionPool.h"

#include <QCryptographicHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>

namespace mfinlogs::data {

ConnectionPool& ConnectionPool::instance() {
    static ConnectionPool pool;
    return pool;
}

ConnectionPool::~ConnectionPool() {
    closeAll();
}

QString ConnectionPool::configFingerprint(const domain::DatabaseConfig& config) {
    const QString raw = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(config.server.trimmed().toLower(),
             config.database.trimmed().toLower(),
             config.username.trimmed().toLower(),
             config.driver.trimmed().toLower(),
             config.useWindowsAuth ? QStringLiteral("win") : QStringLiteral("sql"));
    return QStringLiteral("mfp_%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).left(16).toHex()));
}

QString ConnectionPool::buildConnectionString(const domain::DatabaseConfig& config) {
    auto odbcValue = [](const QString& value) -> QString {
        if (!value.contains(QLatin1Char(';')) && !value.contains(QLatin1Char('{')) && !value.contains(QLatin1Char('}'))) {
            return value;
        }
        return QStringLiteral("{%1}").arg(QString(value).replace(QStringLiteral("}"), QStringLiteral("}}")));
    };

    QStringList parts;
    parts << QStringLiteral("DRIVER=%1").arg(config.driver);
    parts << QStringLiteral("SERVER=%1").arg(odbcValue(config.server.trimmed()));
    parts << QStringLiteral("DATABASE=%1").arg(odbcValue(config.database.trimmed()));
    parts << QStringLiteral("TrustServerCertificate=yes");

    if (config.useWindowsAuth) {
        parts << QStringLiteral("Trusted_Connection=yes");
    } else {
        parts << QStringLiteral("UID=%1").arg(odbcValue(config.username.trimmed()));
        parts << QStringLiteral("PWD=%1").arg(odbcValue(config.password));
    }

    return parts.join(QStringLiteral(";")) + QStringLiteral(";");
}

bool ConnectionPool::healthCheck(const QString& connectionName) {
    QSqlDatabase db = QSqlDatabase::database(connectionName);
    if (!db.isOpen()) {
        return false;
    }
    QSqlQuery q(db);
    return q.exec(QStringLiteral("SELECT 1"));
}

QString ConnectionPool::acquire(const domain::DatabaseConfig& config) {
    QMutexLocker lock(&mutex_);

    const QString fp = configFingerprint(config);
    const QString threadId = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    const QString cacheKey = QStringLiteral("%1_%2").arg(fp, threadId);

    if (cache_.contains(cacheKey)) {
        const QString connName = cache_[cacheKey];
        if (QSqlDatabase::contains(connName) && healthCheck(connName)) {
            refcounts_[connName] = refcounts_.value(connName, 0) + 1;
            return connName;
        }
        if (QSqlDatabase::contains(connName)) {
            {
                QSqlDatabase db = QSqlDatabase::database(connName);
                if (db.isOpen()) {
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connName);
        }
        cache_.remove(cacheKey);
        refcounts_.remove(connName);
    }

    const QString connName = cacheKey;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"), connName);
        db.setDatabaseName(buildConnectionString(config));
        db.setConnectOptions(QStringLiteral("SQL_ATTR_LOGIN_TIMEOUT=5;SQL_ATTR_CONNECTION_TIMEOUT=10"));
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connName);
            return QString();
        }
    }

    cache_.insert(cacheKey, connName);
    refcounts_.insert(connName, 1);
    return connName;
}

void ConnectionPool::release(const QString& connectionName) {
    QMutexLocker lock(&mutex_);

    auto it = refcounts_.find(connectionName);
    if (it == refcounts_.end()) {
        return;
    }

    it.value() -= 1;
    if (it.value() <= 0) {
        refcounts_.erase(it);

        for (auto cit = cache_.begin(); cit != cache_.end(); ++cit) {
            if (cit.value() == connectionName) {
                cache_.erase(cit);
                break;
            }
        }

        if (QSqlDatabase::contains(connectionName)) {
            {
                QSqlDatabase db = QSqlDatabase::database(connectionName);
                if (db.isOpen()) {
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connectionName);
        }
    }
}

void ConnectionPool::markInitialized(const domain::DatabaseConfig& config) {
    QMutexLocker lock(&mutex_);
    initialized_.insert(configFingerprint(config));
}

bool ConnectionPool::isInitialized(const domain::DatabaseConfig& config) const {
    QMutexLocker lock(&mutex_);
    return initialized_.contains(configFingerprint(config));
}

void ConnectionPool::closeAll() {
    QMutexLocker lock(&mutex_);

    for (const QString& connName : cache_) {
        if (QSqlDatabase::contains(connName)) {
            {
                QSqlDatabase db = QSqlDatabase::database(connName);
                if (db.isOpen()) {
                    db.close();
                }
            }
            QSqlDatabase::removeDatabase(connName);
        }
    }
    cache_.clear();
    refcounts_.clear();
    initialized_.clear();
}

} // namespace mfinlogs::data
