#include "data/MigrationService.h"

#include "data/SqlDatabase.h"
#include "data/SqliteSchemaInitializer.h"
#include "domain/DomainErrors.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QUuid>
#include <QVariant>

namespace mfinlogs::data {

namespace {

void executeSqlite(QSqlQuery& query, const QString& action) {
    if (!query.exec()) {
        throw domain::DomainError(QStringLiteral("Migration %1 failed: %2")
            .arg(action, query.lastError().text()).toStdString());
    }
}

// Copy a simple table row-by-row from SQL Server to SQLite.
// columns: list of column names to copy.
void copyTable(QSqlDatabase& src, QSqlDatabase& dst,
               const QString& tableName, const QStringList& columns) {
    // Read from source
    QSqlQuery readQuery(src);
    const QString selectSql = QStringLiteral("SELECT %1 FROM %2")
        .arg(columns.join(QStringLiteral(",")), tableName);
    if (!readQuery.exec(selectSql)) {
        // Table might not exist in source — skip silently
        return;
    }

    // Prepare insert for destination
    QStringList placeholders;
    for (int i = 0; i < columns.size(); ++i) {
        placeholders.append(QStringLiteral("?"));
    }
    // INSERT OR REPLACE so source rows overwrite any bootstrap rows the schema
    // initializer seeded (critically the admin user, pre-created with NULL pw).
    const QString insertSql = QStringLiteral("INSERT OR REPLACE INTO %1 (%2) VALUES (%3)")
        .arg(tableName, columns.join(QStringLiteral(",")), placeholders.join(QStringLiteral(",")));

    QSqlQuery insertQuery(dst);
    insertQuery.prepare(insertSql);

    int count = 0;
    while (readQuery.next()) {
        for (int i = 0; i < columns.size(); ++i) {
            insertQuery.addBindValue(readQuery.value(i));
        }
        executeSqlite(insertQuery, QStringLiteral("Insert into ") + tableName);
        insertQuery.finish();
        insertQuery.prepare(insertSql);
        count += 1;
    }
    Q_UNUSED(count);
}

// Copy transactions with party_id lookup (IDs should match since we copy parties first)
void copyTransactions(QSqlDatabase& src, QSqlDatabase& dst) {
    QSqlQuery readQuery(src);
    if (!readQuery.exec(QStringLiteral(
            "SELECT txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount "
            "FROM transactions ORDER BY txn_id"))) {
        return; // table doesn't exist
    }

    QSqlQuery insertQuery(dst);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    ));

    while (readQuery.next()) {
        // Convert SQL Server date to ISO string for SQLite
        const QVariant dateVal = readQuery.value(0);
        const QString dateStr = dateVal.toDate().isValid()
            ? dateVal.toDate().toString(Qt::ISODate)
            : dateVal.toString();

        insertQuery.addBindValue(dateStr);
        insertQuery.addBindValue(readQuery.value(1));  // bill_no
        insertQuery.addBindValue(readQuery.value(2));  // party_id
        insertQuery.addBindValue(readQuery.value(3));  // txn_type
        insertQuery.addBindValue(readQuery.value(4));  // payment_mode
        insertQuery.addBindValue(readQuery.value(5));  // financial_year
        insertQuery.addBindValue(readQuery.value(6));  // amount
        executeSqlite(insertQuery, QStringLiteral("Insert transaction"));
        insertQuery.finish();
        insertQuery.prepare(QStringLiteral(
            "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"
        ));
    }
}

// Copy inventory items preserving IDs (important for FK references)
void copyInventoryItems(QSqlDatabase& src, QSqlDatabase& dst) {
    QSqlQuery readQuery(src);
    if (!readQuery.exec(QStringLiteral(
            "SELECT id, client_row_id, company, name, cost, min_stock FROM inventory_items ORDER BY id"))) {
        return;
    }

    // SQLite AUTOINCREMENT won't let us set explicit IDs easily in normal mode,
    // but we can INSERT with explicit rowid if we use the id column directly.
    QSqlQuery insertQuery(dst);
    insertQuery.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO inventory_items (id, client_row_id, company, name, cost, min_stock) "
        "VALUES (?, ?, ?, ?, ?, ?)"
    ));

    while (readQuery.next()) {
        insertQuery.addBindValue(readQuery.value(0));  // id (explicit)
        insertQuery.addBindValue(readQuery.value(1));  // client_row_id
        insertQuery.addBindValue(readQuery.value(2));  // company
        insertQuery.addBindValue(readQuery.value(3));  // name
        insertQuery.addBindValue(readQuery.value(4));  // cost
        insertQuery.addBindValue(readQuery.value(5));  // min_stock
        executeSqlite(insertQuery, QStringLiteral("Insert inventory item"));
        insertQuery.finish();
        insertQuery.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO inventory_items (id, client_row_id, company, name, cost, min_stock) "
            "VALUES (?, ?, ?, ?, ?, ?)"
        ));
    }
}

void copyInventoryDayTable(QSqlDatabase& src, QSqlDatabase& dst, const QString& tableName) {
    QSqlQuery readQuery(src);
    if (!readQuery.exec(QStringLiteral("SELECT item_id, company, financial_year, month, day, qty FROM %1").arg(tableName))) {
        return;
    }

    QSqlQuery insertQuery(dst);
    const QString insertSql = QStringLiteral(
        "INSERT INTO %1 (item_id, company, financial_year, month, day, qty) VALUES (?, ?, ?, ?, ?, ?)").arg(tableName);
    insertQuery.prepare(insertSql);

    while (readQuery.next()) {
        insertQuery.addBindValue(readQuery.value(0));
        insertQuery.addBindValue(readQuery.value(1));
        insertQuery.addBindValue(readQuery.value(2));
        insertQuery.addBindValue(readQuery.value(3));
        insertQuery.addBindValue(readQuery.value(4));
        insertQuery.addBindValue(readQuery.value(5));
        executeSqlite(insertQuery, QStringLiteral("Insert ") + tableName);
        insertQuery.finish();
        insertQuery.prepare(insertSql);
    }
}

} // namespace

QString migrateServerToSqlite(const domain::DatabaseConfig& serverConfig, const QString& sqlitePath) {
    // 1. Verify we can connect to SQL Server
    SqlDatabase sqlServer(serverConfig);
    if (!sqlServer.open()) {
        throw domain::DomainError(QStringLiteral("Cannot connect to SQL Server for migration: %1")
            .arg(sqlServer.handle().lastError().text()).toStdString());
    }

    // 2. Remove existing SQLite file if present (fresh migration)
    if (QFile::exists(sqlitePath)) {
        if (!QFile::remove(sqlitePath)) {
            throw domain::DomainError(QStringLiteral("Cannot remove existing SQLite file: %1").arg(sqlitePath).toStdString());
        }
    }

    // 3. Create + initialize SQLite database
    const QString connName = QStringLiteral("migrate-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QSqlDatabase sqliteDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        sqliteDb.setDatabaseName(sqlitePath);
        if (!sqliteDb.open()) {
            QSqlDatabase::removeDatabase(connName);
            throw domain::DomainError(QStringLiteral("Cannot create SQLite database: %1").arg(sqliteDb.lastError().text()).toStdString());
        }

        // Enable WAL for performance during bulk insert
        QSqlQuery walQuery(sqliteDb);
        walQuery.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        walQuery.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

        SqliteSchemaInitializer initializer(sqliteDb);
        initializer.initialize(QStringLiteral("default"));

        // 4. Begin transaction for bulk copy
        if (!sqliteDb.transaction()) {
            sqliteDb.close();
            QSqlDatabase::removeDatabase(connName);
            throw domain::DomainError("Cannot start SQLite transaction for migration");
        }

        QSqlDatabase srcDb = sqlServer.handle();

        // 5. Copy tables in dependency order
        // users
        copyTable(srcDb, sqliteDb, QStringLiteral("users"),
            {QStringLiteral("username"), QStringLiteral("password_hash"), QStringLiteral("role")});

        // app_settings
        copyTable(srcDb, sqliteDb, QStringLiteral("app_settings"),
            {QStringLiteral("setting_key"), QStringLiteral("setting_value")});

        // parties (with explicit party_id to preserve FK references)
        {
            QSqlQuery readParties(srcDb);
            if (readParties.exec(QStringLiteral("SELECT party_id, name, normalized_name, type, credit_allowed FROM parties ORDER BY party_id"))) {
                QSqlQuery insertParty(sqliteDb);
                insertParty.prepare(QStringLiteral(
                    "INSERT OR REPLACE INTO parties (party_id, name, normalized_name, type, credit_allowed) VALUES (?, ?, ?, ?, ?)"
                ));
                while (readParties.next()) {
                    insertParty.addBindValue(readParties.value(0));
                    insertParty.addBindValue(readParties.value(1));
                    insertParty.addBindValue(readParties.value(2));
                    insertParty.addBindValue(readParties.value(3));
                    insertParty.addBindValue(readParties.value(4).toInt());
                    executeSqlite(insertParty, QStringLiteral("Insert party"));
                    insertParty.finish();
                    insertParty.prepare(QStringLiteral(
                        "INSERT OR REPLACE INTO parties (party_id, name, normalized_name, type, credit_allowed) VALUES (?, ?, ?, ?, ?)"
                    ));
                }
            }
        }

        // transactions
        copyTransactions(srcDb, sqliteDb);

        // daily_cash
        {
            QSqlQuery readCash(srcDb);
            if (readCash.exec(QStringLiteral("SELECT cash_date, cash_in_hand FROM daily_cash"))) {
                QSqlQuery insertCash(sqliteDb);
                insertCash.prepare(QStringLiteral("INSERT OR REPLACE INTO daily_cash (cash_date, cash_in_hand) VALUES (?, ?)"));
                while (readCash.next()) {
                    const QVariant dateVal = readCash.value(0);
                    const QString dateStr = dateVal.toDate().isValid()
                        ? dateVal.toDate().toString(Qt::ISODate)
                        : dateVal.toString();
                    insertCash.addBindValue(dateStr);
                    insertCash.addBindValue(readCash.value(1));
                    executeSqlite(insertCash, QStringLiteral("Insert daily_cash"));
                    insertCash.finish();
                    insertCash.prepare(QStringLiteral("INSERT OR REPLACE INTO daily_cash (cash_date, cash_in_hand) VALUES (?, ?)"));
                }
            }
        }

        // audit_logs
        copyTable(srcDb, sqliteDb, QStringLiteral("audit_logs"),
            {QStringLiteral("timestamp"), QStringLiteral("username"), QStringLiteral("action"),
             QStringLiteral("details"), QStringLiteral("company")});

        // inventory_items (with explicit IDs)
        copyInventoryItems(srcDb, sqliteDb);

        // inventory_quantities
        copyInventoryDayTable(srcDb, sqliteDb, QStringLiteral("inventory_quantities"));

        // inventory_purchases
        copyInventoryDayTable(srcDb, sqliteDb, QStringLiteral("inventory_purchases"));

        // 6. Commit
        if (!sqliteDb.commit()) {
            sqliteDb.rollback();
            sqliteDb.close();
            QSqlDatabase::removeDatabase(connName);
            throw domain::DomainError(QStringLiteral("Migration commit failed: %1").arg(sqliteDb.lastError().text()).toStdString());
        }

        sqliteDb.close();
    }
    QSqlDatabase::removeDatabase(connName);

    return sqlitePath;
}

} // namespace mfinlogs::data
