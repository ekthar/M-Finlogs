#include "data/SchemaInitializer.h"

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

SchemaInitializer::SchemaInitializer(QSqlDatabase database)
    : database_(database) {}

void SchemaInitializer::initialize(const QString& companyName) {
    if (!database_.isOpen()) {
        throw domain::DomainError("Cannot initialize schema because SQL Server connection is not open");
    }

    ensureCoreTables();
    ensureMigrations(companyName);
    ensureIndexes();
    ensureDefaultSettings(companyName);
}

void SchemaInitializer::executeStatement(const QString& sql) {
    QSqlQuery query(database_);
    if (!query.exec(sql)) {
        const QString message = QStringLiteral("SQL schema statement failed: %1 | SQL: %2")
            .arg(query.lastError().text(), sql);
        throw domain::DomainError(message.toStdString());
    }
}

void SchemaInitializer::ensureCoreTables() {
    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'users') "
        "CREATE TABLE users ("
        "username NVARCHAR(255) PRIMARY KEY,"
        "password_hash NVARCHAR(255),"
        "role NVARCHAR(50)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'audit_logs') "
        "CREATE TABLE audit_logs ("
        "id INT PRIMARY KEY IDENTITY(1,1),"
        "timestamp DATETIME DEFAULT GETDATE(),"
        "username NVARCHAR(255),"
        "action NVARCHAR(255),"
        "details NVARCHAR(MAX),"
        "company NVARCHAR(255)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'app_settings') "
        "CREATE TABLE app_settings ("
        "setting_key NVARCHAR(255) PRIMARY KEY,"
        "setting_value NVARCHAR(MAX)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'daily_cash') "
        "CREATE TABLE daily_cash ("
        "cash_date DATE PRIMARY KEY,"
        "cash_in_hand DECIMAL(18,2),"
        "updated_at DATETIME DEFAULT GETDATE()"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'parties') "
        "CREATE TABLE parties ("
        "party_id INT PRIMARY KEY IDENTITY(1,1),"
        "name NVARCHAR(255),"
        "normalized_name NVARCHAR(255) UNIQUE,"
        "type NVARCHAR(100),"
        "credit_allowed BIT"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'transactions') "
        "CREATE TABLE transactions ("
        "txn_id INT PRIMARY KEY IDENTITY(1,1),"
        "txn_date DATE,"
        "bill_no NVARCHAR(255),"
        "party_id INT,"
        "txn_type NVARCHAR(50),"
        "payment_mode NVARCHAR(50),"
        "financial_year NVARCHAR(9),"
        "amount DECIMAL(18,2),"
        "FOREIGN KEY (party_id) REFERENCES parties (party_id)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'inventory_items') "
        "CREATE TABLE inventory_items ("
        "id INT PRIMARY KEY IDENTITY(1,1),"
        "client_row_id NVARCHAR(128) NULL,"
        "company NVARCHAR(255) NOT NULL,"
        "name NVARCHAR(255) NOT NULL,"
        "cost DECIMAL(18,2) DEFAULT 0,"
        "min_stock DECIMAL(18,2) DEFAULT 0,"
        "updated_at DATETIME DEFAULT GETDATE()"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'inventory_quantities') "
        "CREATE TABLE inventory_quantities ("
        "id INT PRIMARY KEY IDENTITY(1,1),"
        "item_id INT NOT NULL,"
        "company NVARCHAR(255) NOT NULL,"
        "financial_year NVARCHAR(9) NOT NULL,"
        "month INT NOT NULL,"
        "day INT NOT NULL,"
        "qty DECIMAL(18,2) DEFAULT 0,"
        "FOREIGN KEY (item_id) REFERENCES inventory_items(id)"
        ")"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'inventory_purchases') "
        "CREATE TABLE inventory_purchases ("
        "id INT PRIMARY KEY IDENTITY(1,1),"
        "item_id INT NOT NULL,"
        "company NVARCHAR(255) NOT NULL,"
        "financial_year NVARCHAR(9) NOT NULL,"
        "month INT NOT NULL,"
        "day INT NOT NULL,"
        "qty DECIMAL(18,2) DEFAULT 0,"
        "FOREIGN KEY (item_id) REFERENCES inventory_items(id)"
        ")"
    ));
}

void SchemaInitializer::ensureMigrations(const QString& companyName) {
    Q_UNUSED(companyName);

    executeStatement(QStringLiteral(
        "IF NOT EXISTS ("
        "SELECT * FROM sys.columns WHERE object_id = OBJECT_ID('transactions') AND name = 'financial_year'"
        ") ALTER TABLE transactions ADD financial_year NVARCHAR(9) NULL"
    ));

    executeStatement(QStringLiteral(
        "UPDATE transactions "
        "SET financial_year = CASE "
        "WHEN MONTH(txn_date) >= 4 THEN CONCAT(YEAR(txn_date), '-', YEAR(txn_date) + 1) "
        "ELSE CONCAT(YEAR(txn_date) - 1, '-', YEAR(txn_date)) "
        "END "
        "WHERE financial_year IS NULL AND txn_date IS NOT NULL"
    ));

    executeStatement(QStringLiteral(
        "IF NOT EXISTS ("
        "SELECT * FROM sys.columns WHERE object_id = OBJECT_ID('audit_logs') AND name = 'company'"
        ") ALTER TABLE audit_logs ADD company NVARCHAR(255)"
    ));
}

void SchemaInitializer::ensureIndexes() {
    const QStringList statements = {
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_party_id' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_party_id ON transactions(party_id)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_date ON transactions(txn_date)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_type' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_type ON transactions(txn_type)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_mode' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_mode ON transactions(payment_mode)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_bill_no' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_bill_no ON transactions(bill_no)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date_id' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_date_id ON transactions(txn_date DESC, txn_id DESC)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_date_type_mode' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_date_type_mode ON transactions(txn_date, txn_type, payment_mode)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_transactions_fy_date' AND object_id = OBJECT_ID('transactions')) CREATE INDEX idx_transactions_fy_date ON transactions(financial_year, txn_date)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_parties_normalized' AND object_id = OBJECT_ID('parties')) CREATE INDEX idx_parties_normalized ON parties(normalized_name)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_inventory_items_company_id' AND object_id = OBJECT_ID('inventory_items')) CREATE INDEX idx_inventory_items_company_id ON inventory_items(company, id)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_inventory_items_company_client_row' AND object_id = OBJECT_ID('inventory_items')) CREATE INDEX idx_inventory_items_company_client_row ON inventory_items(company, client_row_id)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_inventory_quantities_lookup' AND object_id = OBJECT_ID('inventory_quantities')) CREATE INDEX idx_inventory_quantities_lookup ON inventory_quantities(company, financial_year, month, item_id)"),
        QStringLiteral("IF NOT EXISTS (SELECT * FROM sys.indexes WHERE name = 'idx_inventory_purchases_lookup' AND object_id = OBJECT_ID('inventory_purchases')) CREATE INDEX idx_inventory_purchases_lookup ON inventory_purchases(company, financial_year, month, item_id)")
    };

    for (const QString& statement : statements) {
        executeStatement(statement);
    }
}

void SchemaInitializer::ensureDefaultSettings(const QString& companyName) {
    QSqlQuery adminQuery(database_);
    adminQuery.prepare(QStringLiteral(
        "IF NOT EXISTS (SELECT 1 FROM users WHERE username = ?) "
        "INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)"
    ));
    adminQuery.addBindValue(QStringLiteral("admin"));
    adminQuery.addBindValue(QStringLiteral("admin"));
    adminQuery.addBindValue(QVariant());
    adminQuery.addBindValue(QStringLiteral("admin"));
    if (!adminQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create bootstrap admin row: %1").arg(adminQuery.lastError().text()).toStdString());
    }

    QSqlQuery companyQuery(database_);
    companyQuery.prepare(QStringLiteral(
        "IF NOT EXISTS (SELECT 1 FROM app_settings WHERE setting_key = ?) "
        "INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
    ));
    companyQuery.addBindValue(QStringLiteral("company_name"));
    companyQuery.addBindValue(QStringLiteral("company_name"));
    companyQuery.addBindValue(companyName);
    if (!companyQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create company setting: %1").arg(companyQuery.lastError().text()).toStdString());
    }

    QSqlQuery fyQuery(database_);
    fyQuery.prepare(QStringLiteral(
        "IF NOT EXISTS (SELECT 1 FROM app_settings WHERE setting_key = ?) "
        "INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
    ));
    fyQuery.addBindValue(QStringLiteral("active_financial_year"));
    fyQuery.addBindValue(QStringLiteral("active_financial_year"));
    fyQuery.addBindValue(financialYearForDate(QDate::currentDate()));
    if (!fyQuery.exec()) {
        throw domain::DomainError(QStringLiteral("Could not create financial year setting: %1").arg(fyQuery.lastError().text()).toStdString());
    }
}

} // namespace mfinlogs::data
