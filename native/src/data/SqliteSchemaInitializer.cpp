#include "data/SqliteSchemaInitializer.h"

#include "domain/DomainErrors.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace mfinlogs::data {

namespace {

QString financialYearForDate(const QDate& date) {
    const int year = date.year();
    const int startYear = date.month() >= 4 ? year : year - 1;
    return QStringLiteral("%1-%2").arg(startYear).arg(startYear + 1);
}

} // namespace

SqliteSchemaInitializer::SqliteSchemaInitializer(QSqlDatabase database)
    : database_(database) {}

void SqliteSchemaInitializer::initialize(const QString& companyName) {
    if (!database_.isOpen()) {
        throw domain::DomainError("Cannot initialize schema because SQLite connection is not open");
    }
    ensureCoreTables();
    ensureIndexes();
    ensureDefaultSettings(companyName);
}

void SqliteSchemaInitializer::executeStatement(const QString& sql) {
    QSqlQuery query(database_);
    if (!query.exec(sql)) {
        const QString message = QStringLiteral("SQLite schema statement failed: %1 | SQL: %2")
            .arg(query.lastError().text(), sql);
        throw domain::DomainError(message.toStdString());
    }
}

void SqliteSchemaInitializer::ensureCoreTables() {
    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password_hash TEXT,"
        "role TEXT"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS audit_logs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp TEXT DEFAULT (datetime('now')),"
        "username TEXT,"
        "action TEXT,"
        "details TEXT,"
        "company TEXT"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "setting_key TEXT PRIMARY KEY,"
        "setting_value TEXT"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS daily_cash ("
        "cash_date TEXT PRIMARY KEY,"
        "cash_in_hand REAL,"
        "updated_at TEXT DEFAULT (datetime('now'))"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS parties ("
        "party_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "normalized_name TEXT UNIQUE,"
        "type TEXT,"
        "credit_allowed INTEGER DEFAULT 0"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS transactions ("
        "txn_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "txn_date TEXT,"
        "bill_no TEXT,"
        "party_id INTEGER,"
        "txn_type TEXT,"
        "payment_mode TEXT,"
        "financial_year TEXT,"
        "amount REAL,"
        "FOREIGN KEY (party_id) REFERENCES parties (party_id)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS inventory_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "client_row_id TEXT,"
        "company TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "cost REAL DEFAULT 0,"
        "min_stock REAL DEFAULT 0,"
        "updated_at TEXT DEFAULT (datetime('now'))"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS inventory_quantities ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "item_id INTEGER NOT NULL,"
        "company TEXT NOT NULL,"
        "financial_year TEXT NOT NULL,"
        "month INTEGER NOT NULL,"
        "day INTEGER NOT NULL,"
        "qty REAL DEFAULT 0,"
        "FOREIGN KEY (item_id) REFERENCES inventory_items(id)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS inventory_purchases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "item_id INTEGER NOT NULL,"
        "company TEXT NOT NULL,"
        "financial_year TEXT NOT NULL,"
        "month INTEGER NOT NULL,"
        "day INTEGER NOT NULL,"
        "qty REAL DEFAULT 0,"
        "FOREIGN KEY (item_id) REFERENCES inventory_items(id)"
        ")"
    ));
}

void SqliteSchemaInitializer::ensureIndexes() {
    const QStringList statements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_party_id ON transactions(party_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_date ON transactions(txn_date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_type ON transactions(txn_type)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_mode ON transactions(payment_mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_bill_no ON transactions(bill_no)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_date_id ON transactions(txn_date DESC, txn_id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_date_type_mode ON transactions(txn_date, txn_type, payment_mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_transactions_fy_date ON transactions(financial_year, txn_date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_parties_normalized ON parties(normalized_name)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_inventory_items_company_id ON inventory_items(company, id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_inventory_items_company_client_row ON inventory_items(company, client_row_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_inventory_quantities_lookup ON inventory_quantities(company, financial_year, month, item_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_inventory_purchases_lookup ON inventory_purchases(company, financial_year, month, item_id)")
    };
    for (const QString& statement : statements) {
        executeStatement(statement);
    }
}

void SqliteSchemaInitializer::ensureDefaultSettings(const QString& companyName) {
    QSqlQuery adminQuery(database_);
    adminQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO users (username, password_hash, role) VALUES (?, NULL, ?)"
    ));
    adminQuery.addBindValue(QStringLiteral("admin"));
    adminQuery.addBindValue(QStringLiteral("admin"));
    if (!adminQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create bootstrap admin row: %1").arg(adminQuery.lastError().text()).toStdString());
    }

    QSqlQuery companyQuery(database_);
    companyQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
    ));
    companyQuery.addBindValue(QStringLiteral("company_name"));
    companyQuery.addBindValue(companyName);
    if (!companyQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create company setting: %1").arg(companyQuery.lastError().text()).toStdString());
    }

    QSqlQuery fyQuery(database_);
    fyQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
    ));
    fyQuery.addBindValue(QStringLiteral("active_financial_year"));
    fyQuery.addBindValue(financialYearForDate(QDate::currentDate()));
    if (!fyQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create financial year setting: %1").arg(fyQuery.lastError().text()).toStdString());
    }
}

} // namespace mfinlogs::data
