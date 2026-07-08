#include "data/SqliteBusinessServices.h"

#include "data/SqliteSchemaInitializer.h"
#include "domain/DomainErrors.h"

#include "data/PasswordVerifier.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariant>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <memory>

namespace mfinlogs::data {
namespace {


// ---------------------------------------------------------------------------
// Helper: RAII SQLite connection wrapper
// ---------------------------------------------------------------------------
class SqliteDb {
public:
    explicit SqliteDb(const QString& path)
        : name_(QUuid::createUuid().toString()) {
        db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name_);
        db_.setDatabaseName(path);
    }
    ~SqliteDb() {
        db_.close();
        QSqlDatabase::removeDatabase(name_);
    }
    bool open() { return db_.open(); }
    QSqlDatabase& handle() { return db_; }
private:
    QString name_;
    QSqlDatabase db_;
};


// ---------------------------------------------------------------------------
// Utility functions
// ---------------------------------------------------------------------------
QString normalizeKey(const QString& value) {
    return value.trimmed().toLower().replace(QStringLiteral(" "), QStringLiteral("_"));
}

QString financialYearForDate(const QDate& date) {
    const int startYear = date.month() >= 4 ? date.year() : date.year() - 1;
    return QStringLiteral("%1-%2").arg(startYear).arg(startYear + 1);
}

QPair<QDate, QDate> financialYearBounds(const QString& financialYear) {
    const QStringList parts = financialYear.split(QStringLiteral("-"));
    if (parts.size() != 2) {
        throw domain::DomainError(QStringLiteral("Invalid financial year: %1").arg(financialYear).toStdString());
    }
    bool ok = false;
    const int startYear = parts.at(0).toInt(&ok);
    if (!ok) {
        throw domain::DomainError(QStringLiteral("Invalid financial year start: %1").arg(financialYear).toStdString());
    }
    return {QDate(startYear, 4, 1), QDate(startYear + 1, 3, 31)};
}


QString roleToString(domain::UserRole role) {
    if (role == domain::UserRole::Admin) {
        return QStringLiteral("admin");
    }
    return QStringLiteral("accounts");
}

domain::UserRole roleFromString(const QString& role) {
    if (role.trimmed().compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0) {
        return domain::UserRole::Admin;
    }
    return domain::UserRole::Accounts;
}

QString transactionTypeToString(domain::TransactionType type) {
    switch (type) {
    case domain::TransactionType::Sale: return QStringLiteral("Sale");
    case domain::TransactionType::SaleReturn: return QStringLiteral("Sale Return");
    case domain::TransactionType::Purchase: return QStringLiteral("Purchase");
    case domain::TransactionType::Expense: return QStringLiteral("Expense");
    case domain::TransactionType::Receipt: return QStringLiteral("Receipt");
    }
    throw domain::DomainError("Unknown transaction type");
}


domain::TransactionType transactionTypeFromString(const QString& type) {
    const QString normalized = type.trimmed().toLower();
    if (normalized == QStringLiteral("sale")) return domain::TransactionType::Sale;
    if (normalized == QStringLiteral("sale return") || normalized == QStringLiteral("sales return") || normalized == QStringLiteral("return"))
        return domain::TransactionType::SaleReturn;
    if (normalized == QStringLiteral("purchase")) return domain::TransactionType::Purchase;
    if (normalized == QStringLiteral("expense")) return domain::TransactionType::Expense;
    if (normalized == QStringLiteral("receipt") || normalized == QStringLiteral("reciept"))
        return domain::TransactionType::Receipt;
    throw domain::DomainError(QStringLiteral("Unsupported transaction type: %1").arg(type).toStdString());
}

QString paymentModeToString(domain::PaymentMode mode) {
    switch (mode) {
    case domain::PaymentMode::Credit: return QStringLiteral("Credit");
    case domain::PaymentMode::Cash: return QStringLiteral("Cash");
    case domain::PaymentMode::Upi: return QStringLiteral("UPI");
    case domain::PaymentMode::Bank: return QStringLiteral("Bank");
    }
    throw domain::DomainError("Unknown payment mode");
}

domain::PaymentMode paymentModeFromString(const QString& mode) {
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("credit")) return domain::PaymentMode::Credit;
    if (normalized == QStringLiteral("cash")) return domain::PaymentMode::Cash;
    if (normalized == QStringLiteral("upi") || normalized == QStringLiteral("gpay") || normalized == QStringLiteral("google pay") || normalized == QStringLiteral("googlepay"))
        return domain::PaymentMode::Upi;
    if (normalized == QStringLiteral("bank")) return domain::PaymentMode::Bank;
    throw domain::DomainError(QStringLiteral("Unsupported payment mode: %1").arg(mode).toStdString());
}


QString sha256Hex(const QString& value) {
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void throwSqlError(const QString& action, const QSqlQuery& query) {
    throw domain::DomainError(QStringLiteral("%1 failed: %2").arg(action, query.lastError().text()).toStdString());
}

void executePrepared(QSqlQuery& query, const QString& action) {
    if (!query.exec()) {
        throwSqlError(action, query);
    }
}

// ---------------------------------------------------------------------------
// Base class for SQLite services
// ---------------------------------------------------------------------------
class SqliteServiceBase {
public:
    explicit SqliteServiceBase(QString dbPath, std::shared_ptr<domain::ConfigService> configService)
        : dbPath_(std::move(dbPath)), configService_(std::move(configService)) {}

protected:
    SqliteDb openDb() const {
        SqliteDb db(dbPath_);
        if (!db.open()) {
            throw domain::DomainError("Could not open SQLite database");
        }
        SqliteSchemaInitializer(db.handle()).initialize(QStringLiteral("default"));
        return db;
    }

    QString selectedFinancialYear(QSqlDatabase database) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
        query.addBindValue(QStringLiteral("active_financial_year"));
        executePrepared(query, QStringLiteral("Read selected financial year"));
        if (query.next() && !query.value(0).toString().trimmed().isEmpty()) {
            return query.value(0).toString();
        }
        return financialYearForDate(QDate::currentDate());
    }


    void setTextSetting(QSqlDatabase database, const QString& key, const QString& value) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
        ));
        query.addBindValue(key);
        query.addBindValue(value);
        executePrepared(query, QStringLiteral("Write app setting"));
    }

    int getOrCreatePartyId(QSqlDatabase database, const QString& name, const QString& type, bool creditAllowed) const {
        const QString normalized = normalizeKey(name);
        if (normalized.isEmpty()) {
            throw domain::DomainError("Party name is required");
        }
        QSqlQuery selectQuery(database);
        selectQuery.prepare(QStringLiteral("SELECT party_id FROM parties WHERE normalized_name=?"));
        selectQuery.addBindValue(normalized);
        executePrepared(selectQuery, QStringLiteral("Read party"));
        if (selectQuery.next()) {
            return selectQuery.value(0).toInt();
        }
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral("INSERT INTO parties (name, normalized_name, type, credit_allowed) VALUES (?, ?, ?, ?)"));
        insertQuery.addBindValue(name.trimmed());
        insertQuery.addBindValue(normalized);
        insertQuery.addBindValue(type.trimmed());
        insertQuery.addBindValue(creditAllowed ? 1 : 0);
        executePrepared(insertQuery, QStringLiteral("Create party"));
        return static_cast<int>(insertQuery.lastInsertId().toLongLong());
    }


    int getExistingPartyId(QSqlDatabase database, const QString& name) const {
        const QString normalized = normalizeKey(name);
        if (normalized.isEmpty()) {
            throw domain::DomainError("Party name is required");
        }
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT party_id FROM parties WHERE normalized_name=?"));
        query.addBindValue(normalized);
        executePrepared(query, QStringLiteral("Read existing party"));
        if (!query.next()) {
            throw domain::DomainError(QStringLiteral("Party not found: %1. Create it from Add Party before saving a transaction.").arg(name).toStdString());
        }
        return query.value(0).toInt();
    }

    void writeAudit(QSqlDatabase database, const QString& username, const QString& action, const QString& details) const {
        QString company;
        if (!cachedCompany_.isEmpty()) {
            company = cachedCompany_;
        } else {
            QSqlQuery companyQuery(database);
            companyQuery.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
            companyQuery.addBindValue(QStringLiteral("company_name"));
            executePrepared(companyQuery, QStringLiteral("Read company for audit"));
            if (companyQuery.next() && !companyQuery.value(0).toString().isEmpty()) {
                company = companyQuery.value(0).toString();
                cachedCompany_ = company;
            } else {
                company = QStringLiteral("default");
            }
        }
        QSqlQuery query(database);
        query.prepare(QStringLiteral("INSERT INTO audit_logs (username, action, details, company) VALUES (?, ?, ?, ?)"));
        query.addBindValue(username);
        query.addBindValue(action);
        query.addBindValue(details);
        query.addBindValue(company);
        executePrepared(query, QStringLiteral("Write audit log"));
    }

    double numericSetting(QSqlDatabase database, const QString& key, double defaultValue) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
        query.addBindValue(key);
        executePrepared(query, QStringLiteral("Read numeric setting"));
        if (!query.next() || query.value(0).toString().trimmed().isEmpty()) {
            return defaultValue;
        }
        bool ok = false;
        const double value = query.value(0).toString().toDouble(&ok);
        return ok ? value : defaultValue;
    }

    const QString& dbPath() const { return dbPath_; }

private:
    QString dbPath_;
    std::shared_ptr<domain::ConfigService> configService_;
    mutable QString cachedCompany_;
};


// ---------------------------------------------------------------------------
// AuthService
// ---------------------------------------------------------------------------
class LiteAuthService final : public domain::AuthService, private SqliteServiceBase {
public:
    LiteAuthService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    bool setupRequired() const override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("SELECT password_hash FROM users WHERE username=?"));
        query.addBindValue(QStringLiteral("admin"));
        executePrepared(query, QStringLiteral("Read admin setup status"));
        return !query.next() || query.value(0).isNull() || query.value(0).toString().isEmpty();
    }

    domain::Session login(const QString& username, const QString& password) override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("SELECT role, password_hash FROM users WHERE username=?"));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Read login user"));
        if (!query.next()) {
            throw domain::DomainError(QStringLiteral("Invalid credentials for user: %1").arg(username).toStdString());
        }
        const QString role = query.value(0).toString();
        const QString storedHash = query.value(1).toString();

        // Verify against SHA-256 (native) or Argon2id (Electron/Python backend).
        if (storedHash.isEmpty() || !verifyPassword(storedHash, password)) {
            throw domain::DomainError(QStringLiteral("Invalid credentials for user: %1").arg(username).toStdString());
        }

        // Upgrade legacy Argon2 hashes to native SHA-256 on successful login.
        if (storedHash.startsWith(QStringLiteral("$argon2"))) {
            QSqlQuery updateQuery(db.handle());
            updateQuery.prepare(QStringLiteral("UPDATE users SET password_hash=? WHERE username=?"));
            updateQuery.addBindValue(sha256Hex(password));
            updateQuery.addBindValue(username);
            executePrepared(updateQuery, QStringLiteral("Upgrade password hash to SHA-256"));
        }

        writeAudit(db.handle(), username, QStringLiteral("Login"), QStringLiteral("User logged in"));
        return domain::Session{username, roleFromString(role), QStringLiteral("native:%1:%2").arg(username, role)};
    }


    void setupAdmin(const QString& username, const QString& password) override {
        if (username.trimmed().isEmpty()) {
            throw domain::DomainError("Admin username is required");
        }
        if (password.size() < 8) {
            throw domain::DomainError("Admin password must be at least 8 characters");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO users (username, password_hash, role) VALUES (?, ?, ?)"
        ));
        query.addBindValue(username.trimmed());
        query.addBindValue(sha256Hex(password));
        query.addBindValue(QStringLiteral("admin"));
        executePrepared(query, QStringLiteral("Setup admin"));
        writeAudit(db.handle(), QStringLiteral("system"), QStringLiteral("Setup"), QStringLiteral("Admin password configured"));
    }
};


// ---------------------------------------------------------------------------
// UserService
// ---------------------------------------------------------------------------
class LiteUserService final : public domain::UserService, private SqliteServiceBase {
public:
    LiteUserService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray listUsers(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        if (!query.exec(QStringLiteral("SELECT username, role FROM users ORDER BY username"))) {
            throwSqlError(QStringLiteral("List users"), query);
        }
        QJsonArray users;
        while (query.next()) {
            QJsonObject user;
            user.insert(QStringLiteral("username"), query.value(0).toString());
            user.insert(QStringLiteral("role"), query.value(1).toString());
            users.append(user);
        }
        return users;
    }

    void createUser(const QString& username, const QString& password, domain::UserRole role, const QString& adminToken) override {
        Q_UNUSED(adminToken);
        if (username.trimmed().isEmpty()) {
            throw domain::DomainError("Username is required");
        }
        if (password.size() < 8) {
            throw domain::DomainError("Password must be at least 8 characters");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)"));
        query.addBindValue(username.trimmed());
        query.addBindValue(sha256Hex(password));
        query.addBindValue(roleToString(role));
        executePrepared(query, QStringLiteral("Create user"));
        writeAudit(db.handle(), QStringLiteral("native-admin"), QStringLiteral("Create User"), QStringLiteral("Created user ") + username);
    }


    void changePassword(const QString& username, const QString& password, const QString& adminToken) override {
        Q_UNUSED(adminToken);
        if (password.size() < 8) {
            throw domain::DomainError("Password must be at least 8 characters");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("UPDATE users SET password_hash=? WHERE username=?"));
        query.addBindValue(sha256Hex(password));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Change user password"));
        if (query.numRowsAffected() == 0) {
            throw domain::DomainError(QStringLiteral("User not found: %1").arg(username).toStdString());
        }
        writeAudit(db.handle(), QStringLiteral("native-admin"), QStringLiteral("Change Password"), QStringLiteral("Password changed for ") + username);
    }

    void deleteUser(const QString& username, const QString& adminToken) override {
        Q_UNUSED(adminToken);
        if (username == QStringLiteral("admin")) {
            throw domain::DomainError("Cannot delete default admin");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("DELETE FROM users WHERE username=?"));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Delete user"));
        writeAudit(db.handle(), QStringLiteral("native-admin"), QStringLiteral("Delete User"), QStringLiteral("Deleted user ") + username);
    }
};


// ---------------------------------------------------------------------------
// CompanyService
// ---------------------------------------------------------------------------
class LiteCompanyService final : public domain::CompanyService, private SqliteServiceBase {
public:
    LiteCompanyService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray listCompanies() override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
        query.addBindValue(QStringLiteral("company_name"));
        executePrepared(query, QStringLiteral("Read company"));
        const QString company = query.next() && !query.value(0).toString().isEmpty()
            ? query.value(0).toString()
            : QStringLiteral("default");
        QJsonObject item;
        item.insert(QStringLiteral("name"), company);
        item.insert(QStringLiteral("key"), normalizeKey(company));
        QJsonArray companies;
        companies.append(item);
        return companies;
    }

    void createCompany(const QString& name) override {
        selectCompany(name);
    }

    void selectCompany(const QString& name) override {
        const QString trimmedName = name.trimmed();
        if (trimmedName.isEmpty()) {
            throw domain::DomainError("Company name is required");
        }
        SqliteDb db(dbPath());
        if (!db.open()) {
            throw domain::DomainError("Could not open SQLite database while selecting company");
        }
        SqliteSchemaInitializer(db.handle()).initialize(trimmedName);
        setTextSetting(db.handle(), QStringLiteral("company_name"), trimmedName);
    }
};


// ---------------------------------------------------------------------------
// FinancialYearService
// ---------------------------------------------------------------------------
class LiteFinancialYearService final : public domain::FinancialYearService, private SqliteServiceBase {
public:
    LiteFinancialYearService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray listFinancialYears() override {
        SqliteDb db = openDb();
        QStringList merged;
        const QString selected = SqliteServiceBase::selectedFinancialYear(db.handle());
        merged.append(selected);
        merged.append(financialYearForDate(QDate::currentDate()));

        QSqlQuery query(db.handle());
        if (!query.exec(QStringLiteral("SELECT DISTINCT financial_year FROM transactions WHERE financial_year IS NOT NULL ORDER BY financial_year DESC"))) {
            throwSqlError(QStringLiteral("List financial years"), query);
        }
        while (query.next()) {
            const QString year = query.value(0).toString();
            if (!year.isEmpty() && !merged.contains(year)) {
                merged.append(year);
            }
        }
        merged.sort(Qt::CaseInsensitive);
        std::reverse(merged.begin(), merged.end());
        QJsonArray years;
        for (const QString& year : merged) {
            years.append(year);
        }
        return years;
    }

    void selectFinancialYear(const QString& financialYear) override {
        const QString selected = financialYear.trimmed();
        financialYearBounds(selected); // validate
        SqliteDb db = openDb();
        setTextSetting(db.handle(), QStringLiteral("active_financial_year"), selected);
    }

    QString selectedFinancialYear() const override {
        SqliteDb db = openDb();
        return SqliteServiceBase::selectedFinancialYear(db.handle());
    }
};


// ---------------------------------------------------------------------------
// PartyService
// ---------------------------------------------------------------------------
class LitePartyService final : public domain::PartyService, private SqliteServiceBase {
public:
    LitePartyService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray listParties() override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        if (!query.exec(QStringLiteral("SELECT name, type FROM parties ORDER BY name"))) {
            throwSqlError(QStringLiteral("List parties"), query);
        }
        QJsonArray parties;
        while (query.next()) {
            QJsonObject party;
            party.insert(QStringLiteral("name"), query.value(0).toString());
            party.insert(QStringLiteral("type"), query.value(1).toString());
            parties.append(party);
        }
        return parties;
    }

    void createParty(const QString& name, const QString& type, bool creditAllowed) override {
        SqliteDb db = openDb();
        if (type == QStringLiteral("Bank")) {
            QSqlQuery countQuery(db.handle());
            if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM parties WHERE type = 'Bank'"))) {
                throwSqlError(QStringLiteral("Count bank parties"), countQuery);
            }
            countQuery.next();
            if (countQuery.value(0).toInt() > 0) {
                throw domain::DomainError("Only one Bank account is allowed");
            }
        }
        getOrCreatePartyId(db.handle(), name, type, creditAllowed);
    }

    void renameParty(const QString& oldName, const QString& newName, const QString& adminUser) override {
        const QString normalizedOldName = normalizeKey(oldName);
        const QString normalizedNewName = normalizeKey(newName);
        if (normalizedOldName.isEmpty() || normalizedNewName.isEmpty()) {
            throw domain::DomainError("Old and new party names are required");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("UPDATE parties SET name=?, normalized_name=? WHERE normalized_name=?"));
        query.addBindValue(newName.trimmed());
        query.addBindValue(normalizedNewName);
        query.addBindValue(normalizedOldName);
        executePrepared(query, QStringLiteral("Rename party"));
        if (query.numRowsAffected() == 0) {
            throw domain::DomainError(QStringLiteral("Party not found: %1").arg(oldName).toStdString());
        }
        writeAudit(db.handle(), adminUser, QStringLiteral("Rename Party"),
            QStringLiteral("Renamed party from ") + oldName + QStringLiteral(" to ") + newName);
    }
};


// ---------------------------------------------------------------------------
// TransactionService
// ---------------------------------------------------------------------------
class LiteTransactionService final : public domain::TransactionService, private SqliteServiceBase {
public:
    LiteTransactionService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    int addTransaction(const domain::TransactionCreateRequest& request) override {
        SqliteDb db = openDb();
        const int partyId = getExistingPartyId(db.handle(), request.party);
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"
        ));
        query.addBindValue(request.date.toString(Qt::ISODate));
        query.addBindValue(request.billNo);
        query.addBindValue(partyId);
        query.addBindValue(transactionTypeToString(request.type));
        query.addBindValue(paymentModeToString(request.mode));
        query.addBindValue(financialYearForDate(request.date));
        query.addBindValue(request.amount);
        executePrepared(query, QStringLiteral("Add transaction"));
        return static_cast<int>(query.lastInsertId().toLongLong());
    }


    QVector<domain::TransactionRow> listTransactions(int page, int limit, int days) override {
        if (page < 1 || limit < 1) {
            throw domain::DomainError("Transaction page and limit must be positive");
        }
        SqliteDb db = openDb();
        const QString financialYear = selectedFinancialYear(db.handle());
        const QPair<QDate, QDate> bounds = financialYearBounds(financialYear);
        QDate lowerBound = bounds.first;
        if (days > 0) {
            lowerBound = std::max(lowerBound, QDate::currentDate().addDays(-days));
        }
        const int offset = (page - 1) * limit;

        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount "
            "FROM transactions t "
            "JOIN parties p ON t.party_id = p.party_id "
            "WHERE t.txn_date >= ? AND t.txn_date <= ? "
            "ORDER BY t.txn_date DESC, t.txn_id DESC "
            "LIMIT ? OFFSET ?"
        ));
        query.addBindValue(lowerBound.toString(Qt::ISODate));
        query.addBindValue(bounds.second.toString(Qt::ISODate));
        query.addBindValue(limit);
        query.addBindValue(offset);
        executePrepared(query, QStringLiteral("List transactions"));

        QVector<domain::TransactionRow> rows;
        while (query.next()) {
            rows.append(domain::TransactionRow{
                query.value(0).toInt(),
                QDate::fromString(query.value(1).toString(), Qt::ISODate),
                query.value(2).toString(),
                query.value(3).toString(),
                transactionTypeFromString(query.value(4).toString()),
                paymentModeFromString(query.value(5).toString()),
                query.value(6).toDouble()
            });
        }
        return rows;
    }


    domain::TransactionRow getTransaction(int transactionId) override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount "
            "FROM transactions t JOIN parties p ON t.party_id = p.party_id WHERE t.txn_id=?"
        ));
        query.addBindValue(transactionId);
        executePrepared(query, QStringLiteral("Read transaction"));
        if (!query.next()) {
            throw domain::DomainError(QStringLiteral("Transaction not found: %1").arg(transactionId).toStdString());
        }
        return domain::TransactionRow{
            query.value(0).toInt(),
            QDate::fromString(query.value(1).toString(), Qt::ISODate),
            query.value(2).toString(),
            query.value(3).toString(),
            transactionTypeFromString(query.value(4).toString()),
            paymentModeFromString(query.value(5).toString()),
            query.value(6).toDouble()
        };
    }


    void editTransaction(const domain::TransactionEditRequest& request) override {
        SqliteDb db = openDb();
        QSqlDatabase handle = db.handle();

        const int partyId = getExistingPartyId(handle, request.data.party);

        if (!handle.transaction()) {
            throw domain::DomainError(QStringLiteral("Could not begin transaction for edit: %1").arg(handle.lastError().text()).toStdString());
        }

        try {
            QSqlQuery query(handle);
            query.prepare(QStringLiteral(
                "UPDATE transactions SET txn_date=?, bill_no=?, party_id=?, txn_type=?, payment_mode=?, financial_year=?, amount=? WHERE txn_id=?"
            ));
            query.addBindValue(request.data.date.toString(Qt::ISODate));
            query.addBindValue(request.data.billNo.trimmed());
            query.addBindValue(partyId);
            query.addBindValue(transactionTypeToString(request.data.type));
            query.addBindValue(paymentModeToString(request.data.mode));
            query.addBindValue(financialYearForDate(request.data.date));
            query.addBindValue(request.data.amount);
            query.addBindValue(request.transactionId);
            executePrepared(query, QStringLiteral("Edit transaction"));

            if (!handle.commit()) {
                throw domain::DomainError(QStringLiteral("Could not commit transaction edit: %1").arg(handle.lastError().text()).toStdString());
            }
        } catch (...) {
            handle.rollback();
            throw;
        }

        writeAudit(handle, request.adminUser, QStringLiteral("Edit Transaction"),
            QStringLiteral("Edited transaction ") + QString::number(request.transactionId));
    }


    void deleteTransaction(const domain::TransactionDeleteRequest& request) override {
        SqliteDb db = openDb();
        QSqlDatabase handle = db.handle();

        if (!handle.transaction()) {
            throw domain::DomainError(QStringLiteral("Could not begin transaction for delete: %1").arg(handle.lastError().text()).toStdString());
        }

        try {
            QSqlQuery query(handle);
            query.prepare(QStringLiteral("DELETE FROM transactions WHERE txn_id=?"));
            query.addBindValue(request.transactionId);
            executePrepared(query, QStringLiteral("Delete transaction"));
            if (query.numRowsAffected() == 0) {
                throw domain::DomainError(QStringLiteral("Transaction not found: %1").arg(request.transactionId).toStdString());
            }

            if (!handle.commit()) {
                throw domain::DomainError(QStringLiteral("Could not commit transaction delete: %1").arg(handle.lastError().text()).toStdString());
            }
        } catch (...) {
            handle.rollback();
            throw;
        }

        writeAudit(handle, request.adminUser, QStringLiteral("Delete Transaction"),
            QStringLiteral("Deleted transaction ") + QString::number(request.transactionId));
    }
};


// ---------------------------------------------------------------------------
// ReportService
// ---------------------------------------------------------------------------
class LiteReportService final : public domain::ReportService, private SqliteServiceBase {
public:
    LiteReportService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonObject dashboardMetrics() override {
        SqliteDb db = openDb();
        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(db.handle()));
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        const int curMonth = QDate::currentDate().month();
        const int curYear = QDate::currentDate().year();

        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN UPPER(TRIM(txn_type))='SALE' AND txn_date = ? THEN amount "
            "     WHEN UPPER(TRIM(txn_type)) IN ('SALE RETURN','SALES RETURN','RETURN') AND txn_date = ? THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(TRIM(txn_type))='SALE' AND CAST(strftime('%m', txn_date) AS INTEGER)=? AND CAST(strftime('%Y', txn_date) AS INTEGER)=? THEN amount "
            "     WHEN UPPER(TRIM(txn_type)) IN ('SALE RETURN','SALES RETURN','RETURN') AND CAST(strftime('%m', txn_date) AS INTEGER)=? AND CAST(strftime('%Y', txn_date) AS INTEGER)=? THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(TRIM(txn_type)) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(TRIM(txn_type)) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        query.addBindValue(today);
        query.addBindValue(today);
        query.addBindValue(curMonth);
        query.addBindValue(curYear);
        query.addBindValue(curMonth);
        query.addBindValue(curYear);
        query.addBindValue(bounds.first.toString(Qt::ISODate));
        query.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read dashboard metrics"));
        query.next();

        const double cashIn = query.value(2).toDouble();
        const double cashOut = query.value(3).toDouble();
        const double bankIn = query.value(4).toDouble();
        const double bankOut = query.value(5).toDouble();

        QJsonObject result;
        result.insert(QStringLiteral("sales_today"), query.value(0).toDouble());
        result.insert(QStringLiteral("sales_month"), query.value(1).toDouble());
        result.insert(QStringLiteral("cash_balance"), cashIn - cashOut);
        result.insert(QStringLiteral("bank_balance"), bankIn - bankOut);
        result.insert(QStringLiteral("receivables"), receivables(db.handle(), bounds));
        return result;
    }


    QJsonObject ledger(const QString& party, const domain::ReportRange& range) override {
        SqliteDb db = openDb();
        QSqlQuery partyQuery(db.handle());
        partyQuery.prepare(QStringLiteral("SELECT party_id FROM parties WHERE normalized_name=?"));
        partyQuery.addBindValue(normalizeKey(party));
        executePrepared(partyQuery, QStringLiteral("Read ledger party"));

        QJsonObject result;
        QJsonArray rows;
        result.insert(QStringLiteral("opening_balance"), 0.0);
        result.insert(QStringLiteral("period_start"), range.start.toString(Qt::ISODate));
        result.insert(QStringLiteral("period_end"), range.end.toString(Qt::ISODate));
        result.insert(QStringLiteral("financial_year"), selectedFinancialYear(db.handle()));
        if (!partyQuery.next()) {
            result.insert(QStringLiteral("data"), rows);
            return result;
        }

        const int partyId = partyQuery.value(0).toInt();
        double openingBalance = 0.0;
        if (range.start.isValid()) {
            QSqlQuery openingQuery(db.handle());
            openingQuery.prepare(QStringLiteral(
                "SELECT SUM(CASE WHEN UPPER(TRIM(txn_type))='SALE' THEN amount "
                "WHEN UPPER(TRIM(txn_type)) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN -amount ELSE 0 END) "
                "FROM transactions WHERE party_id=? AND txn_date < ?"
            ));
            openingQuery.addBindValue(partyId);
            openingQuery.addBindValue(range.start.toString(Qt::ISODate));
            executePrepared(openingQuery, QStringLiteral("Read ledger opening balance"));
            openingQuery.next();
            openingBalance = openingQuery.value(0).toDouble();
        }

        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT txn_id, txn_date, bill_no, txn_type, payment_mode, amount "
            "FROM transactions WHERE party_id=? AND txn_date >= ? AND txn_date <= ? ORDER BY txn_date, txn_id"
        ));
        query.addBindValue(partyId);
        query.addBindValue(range.start.toString(Qt::ISODate));
        query.addBindValue(range.end.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read ledger rows"));

        double balance = openingBalance;
        while (query.next()) {
            const QString type = query.value(3).toString().trimmed().toUpper();
            const double amount = query.value(5).toDouble();
            if (type == QStringLiteral("SALE")) {
                balance += amount;
            } else if (type == QStringLiteral("RECEIPT") || type == QStringLiteral("RECIEPT") ||
                       type == QStringLiteral("SALE RETURN") || type == QStringLiteral("SALES RETURN") ||
                       type == QStringLiteral("RETURN")) {
                balance -= amount;
            }
            QJsonObject item;
            item.insert(QStringLiteral("id"), query.value(0).toInt());
            item.insert(QStringLiteral("date"), query.value(1).toString());
            item.insert(QStringLiteral("bill_no"), query.value(2).toString());
            item.insert(QStringLiteral("type"), query.value(3).toString());
            item.insert(QStringLiteral("mode"), query.value(4).toString());
            item.insert(QStringLiteral("amount"), amount);
            item.insert(QStringLiteral("balance"), balance);
            item.insert(QStringLiteral("financial_year"), result.value(QStringLiteral("financial_year")).toString());
            rows.append(item);
        }
        result.insert(QStringLiteral("opening_balance"), openingBalance);
        result.insert(QStringLiteral("data"), rows);
        return result;
    }


    QJsonArray dayBook(const QDate& date) override {
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE t.txn_date=? ORDER BY t.txn_id"
        ));
        query.addBindValue(date.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read day book"));
        return transactionRowsToJson(query);
    }

    QJsonArray dailySummary(const domain::ReportRange& range) override {
        SqliteDb db = openDb();
        return cashSummaryRows(db.handle(), range);
    }

    QJsonArray shortExcess(const domain::ReportRange& range) override {
        SqliteDb db = openDb();
        return cashSummaryRows(db.handle(), range);
    }


    void saveCashInHand(const QDate& date, double amount) override {
        if (!date.isValid()) {
            throw domain::DomainError("Cash date is required");
        }
        if (amount < 0.0) {
            throw domain::DomainError("Cash in hand cannot be negative");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO daily_cash (cash_date, cash_in_hand, updated_at) VALUES (?, ?, datetime('now'))"
        ));
        query.addBindValue(date.toString(Qt::ISODate));
        query.addBindValue(amount);
        executePrepared(query, QStringLiteral("Save cash in hand"));
    }

    void resetCashInHand(const QDate& date) override {
        if (!date.isValid()) {
            throw domain::DomainError("Cash date is required");
        }
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral("DELETE FROM daily_cash WHERE cash_date=?"));
        query.addBindValue(date.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Reset cash in hand"));
    }

    double openingCashSeed() override {
        SqliteDb db = openDb();
        return numericSetting(db.handle(), QStringLiteral("opening_cash_seed"), 0.0);
    }

    void saveOpeningCashSeed(double amount) override {
        if (amount < 0.0) {
            throw domain::DomainError("Opening cash cannot be negative");
        }
        SqliteDb db = openDb();
        setTextSetting(db.handle(), QStringLiteral("opening_cash_seed"), QString::number(amount, 'f', 2));
    }


    QJsonObject outstanding() override {
        SqliteDb db = openDb();
        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(db.handle()));
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT p.name, p.type, "
            "SUM(CASE WHEN UPPER(TRIM(t.txn_type))='SALE' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(TRIM(t.txn_type)) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END), "
            "MAX(CASE WHEN UPPER(TRIM(t.txn_type)) IN ('RECEIPT','RECIEPT') THEN t.txn_date END), "
            "MIN(CASE WHEN UPPER(TRIM(t.txn_type))='SALE' THEN t.txn_date END) "
            "FROM parties p LEFT JOIN transactions t ON t.party_id=p.party_id AND t.txn_date >= ? AND t.txn_date <= ? "
            "WHERE p.type IN ('Customer','Credit Customer') GROUP BY p.name, p.type ORDER BY p.name"
        ));
        query.addBindValue(bounds.first.toString(Qt::ISODate));
        query.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read outstanding report"));

        QJsonArray rows;
        double total = 0.0;
        int highCount = 0, criticalCount = 0;
        double highAmount = 0.0, criticalAmount = 0.0;
        int maxDaysUnpaid = 0;
        const QDate today = QDate::currentDate();

        while (query.next()) {
            const QString partyName = query.value(0).toString().trimmed();
            if (partyName.compare(QStringLiteral("customer"), Qt::CaseInsensitive) == 0) {
                continue;
            }
            const double balance = query.value(2).toDouble() - query.value(3).toDouble();
            if (std::abs(balance) < 0.005) continue;
            const QDate lastReceipt = QDate::fromString(query.value(4).toString(), Qt::ISODate);
            const QDate firstSale = QDate::fromString(query.value(5).toString(), Qt::ISODate);
            const QDate anchor = lastReceipt.isValid() ? lastReceipt : firstSale;
            const int daysUnpaid = anchor.isValid() ? static_cast<int>(anchor.daysTo(today)) : 0;
            maxDaysUnpaid = qMax(maxDaysUnpaid, daysUnpaid);
            if (daysUnpaid >= 30) { criticalCount += 1; criticalAmount += balance; }
            if (daysUnpaid >= 15) { highCount += 1; highAmount += balance; }

            QJsonObject item;
            item.insert(QStringLiteral("party"), query.value(0).toString());
            item.insert(QStringLiteral("type"), query.value(1).toString());
            item.insert(QStringLiteral("status"), riskLabel(daysUnpaid, lastReceipt.isValid()));
            item.insert(QStringLiteral("days_unpaid"), daysUnpaid);
            item.insert(QStringLiteral("last_receipt"), lastReceipt.isValid() ? lastReceipt.toString(Qt::ISODate) : QStringLiteral("No receipt"));
            item.insert(QStringLiteral("balance"), balance);
            rows.append(item);
            total += balance;
        }

        QJsonObject stats;
        stats.insert(QStringLiteral("high_count"), highCount);
        stats.insert(QStringLiteral("critical_count"), criticalCount);
        stats.insert(QStringLiteral("high_amount"), highAmount);
        stats.insert(QStringLiteral("critical_amount"), criticalAmount);
        stats.insert(QStringLiteral("max_days_unpaid"), maxDaysUnpaid);

        QJsonObject result;
        result.insert(QStringLiteral("data"), rows);
        result.insert(QStringLiteral("total"), total);
        result.insert(QStringLiteral("stats"), stats);
        return result;
    }


    QJsonArray trialBalance() override {
        SqliteDb db = openDb();
        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(db.handle()));

        QSqlQuery modeQuery(db.handle());
        modeQuery.prepare(QStringLiteral(
            "SELECT payment_mode, txn_type, SUM(amount) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ? GROUP BY payment_mode, txn_type"
        ));
        modeQuery.addBindValue(bounds.first.toString(Qt::ISODate));
        modeQuery.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(modeQuery, QStringLiteral("Read trial balance mode totals"));

        double cashIn = 0.0, cashOut = 0.0;
        double bankIn = 0.0, bankOut = 0.0;
        double upiIn = 0.0, upiOut = 0.0;
        while (modeQuery.next()) {
            const QString mode = modeQuery.value(0).toString().trimmed().toLower();
            const QString type = modeQuery.value(1).toString().trimmed().toLower();
            const double amount = modeQuery.value(2).toDouble();
            double* inBucket = &cashIn;
            double* outBucket = &cashOut;
            if (mode == QStringLiteral("bank") || mode == QStringLiteral("gpay") || mode == QStringLiteral("google pay") || mode == QStringLiteral("googlepay")) {
                inBucket = &bankIn; outBucket = &bankOut;
            } else if (mode == QStringLiteral("upi")) {
                inBucket = &upiIn; outBucket = &upiOut;
            }
            if (type == QStringLiteral("sale") || type == QStringLiteral("receipt") || type == QStringLiteral("reciept")) {
                *inBucket += amount;
            } else if (type == QStringLiteral("expense") || type == QStringLiteral("sale return") || type == QStringLiteral("sales return") || type == QStringLiteral("return")) {
                *outBucket += amount;
            }
        }

        QSqlQuery debtorQuery(db.handle());
        debtorQuery.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN t.txn_type IN ('Receipt','Reciept','Sale Return','Sales Return','Return') THEN t.amount ELSE 0 END) "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE p.type='Credit Customer' AND t.txn_date >= ? AND t.txn_date <= ?"
        ));
        debtorQuery.addBindValue(bounds.first.toString(Qt::ISODate));
        debtorQuery.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(debtorQuery, QStringLiteral("Read trial balance debtors"));
        debtorQuery.next();
        const double debtors = debtorQuery.value(0).toDouble() - debtorQuery.value(1).toDouble();

        QSqlQuery profitQuery(db.handle());
        profitQuery.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return','Sales Return','Return') THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        profitQuery.addBindValue(bounds.first.toString(Qt::ISODate));
        profitQuery.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(profitQuery, QStringLiteral("Read trial balance profit totals"));
        profitQuery.next();
        const double sales = profitQuery.value(0).toDouble();
        const double expenses = profitQuery.value(1).toDouble();


        auto accountRow = [](const QString& account, double debit, double credit) {
            QJsonObject item;
            item.insert(QStringLiteral("account"), account);
            item.insert(QStringLiteral("debit"), debit);
            item.insert(QStringLiteral("credit"), credit);
            item.insert(QStringLiteral("balance"), debit - credit);
            return item;
        };
        QJsonArray rows;
        const double cashBalance = cashIn - cashOut;
        const double bankBalance = bankIn - bankOut;
        const double upiBalance = upiIn - upiOut;
        rows.append(accountRow(QStringLiteral("Cash Account"), cashBalance > 0.0 ? cashBalance : 0.0, cashBalance < 0.0 ? -cashBalance : 0.0));
        rows.append(accountRow(QStringLiteral("Bank Account"), bankBalance > 0.0 ? bankBalance : 0.0, bankBalance < 0.0 ? -bankBalance : 0.0));
        rows.append(accountRow(QStringLiteral("UPI Account"), upiBalance > 0.0 ? upiBalance : 0.0, upiBalance < 0.0 ? -upiBalance : 0.0));
        rows.append(accountRow(QStringLiteral("Sundry Debtors"), debtors > 0.0 ? debtors : 0.0, debtors < 0.0 ? -debtors : 0.0));
        rows.append(accountRow(QStringLiteral("Sundry Creditors"), 0.0, 0.0));
        rows.append(accountRow(QStringLiteral("Sales Account"), 0.0, sales));
        rows.append(accountRow(QStringLiteral("Expense Account"), expenses, 0.0));
        return rows;
    }

    QJsonObject profitAndLoss() override {
        SqliteDb db = openDb();
        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(db.handle()));
        QSqlQuery query(db.handle());
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return','Sales Return','Return') THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        query.addBindValue(bounds.first.toString(Qt::ISODate));
        query.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read profit and loss"));
        query.next();
        const double sales = query.value(0).toDouble();
        const double expenses = query.value(1).toDouble();
        QJsonObject result;
        result.insert(QStringLiteral("sales"), sales);
        result.insert(QStringLiteral("expenses"), expenses);
        result.insert(QStringLiteral("net_profit"), sales - expenses);
        return result;
    }


private:
    QString riskLabel(int daysUnpaid, bool hasReceipt) const {
        if (!hasReceipt) return QStringLiteral("No receipt");
        if (daysUnpaid >= 30) return QStringLiteral("Critical");
        if (daysUnpaid >= 15) return QStringLiteral("High");
        return QStringLiteral("Normal");
    }

    QJsonArray cashSummaryRows(QSqlDatabase database, const domain::ReportRange& range) const {
        if (range.start > range.end) {
            throw domain::DomainError("Report start date must be before end date");
        }
        struct CashDay final {
            double cashIn = 0.0;
            double cashExpense = 0.0;
            double bank = 0.0;
            double creditSale = 0.0;
            double totalSales = 0.0;
            bool hasCashInHand = false;
            double cashInHand = 0.0;
        };

        QMap<QDate, CashDay> byDate;
        QSqlQuery txnQuery(database);
        txnQuery.prepare(QStringLiteral(
            "SELECT txn_date, "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN (payment_mode='Credit' AND UPPER(TRIM(txn_type))='SALE') OR UPPER(TRIM(txn_type)) IN ('RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(TRIM(txn_type))='SALE' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ? GROUP BY txn_date"
        ));
        txnQuery.addBindValue(range.start.toString(Qt::ISODate));
        txnQuery.addBindValue(range.end.toString(Qt::ISODate));
        executePrepared(txnQuery, QStringLiteral("Read cash summary transactions"));
        while (txnQuery.next()) {
            CashDay day;
            day.cashIn = txnQuery.value(1).toDouble();
            day.cashExpense = txnQuery.value(2).toDouble();
            day.bank = txnQuery.value(3).toDouble();
            day.creditSale = txnQuery.value(4).toDouble();
            day.totalSales = txnQuery.value(5).toDouble();
            byDate.insert(QDate::fromString(txnQuery.value(0).toString(), Qt::ISODate), day);
        }

        QSqlQuery cashQuery(database);
        cashQuery.prepare(QStringLiteral("SELECT cash_date, cash_in_hand FROM daily_cash WHERE cash_date >= ? AND cash_date <= ?"));
        cashQuery.addBindValue(range.start.toString(Qt::ISODate));
        cashQuery.addBindValue(range.end.toString(Qt::ISODate));
        executePrepared(cashQuery, QStringLiteral("Read daily cash entries"));
        while (cashQuery.next()) {
            const QDate d = QDate::fromString(cashQuery.value(0).toString(), Qt::ISODate);
            CashDay day = byDate.value(d);
            day.hasCashInHand = true;
            day.cashInHand = cashQuery.value(1).toDouble();
            byDate.insert(d, day);
        }


        double previousClosing = openingCashBeforeDate(database, range.start);
        QJsonArray rows;
        for (QDate date = range.start; date <= range.end; date = date.addDays(1)) {
            const CashDay day = byDate.value(date);
            const double openingCash = previousClosing;
            const double cashNeeded = openingCash + day.cashIn - day.cashExpense;
            const double cashInHand = day.hasCashInHand ? day.cashInHand : cashNeeded;
            const double shortExcessVal = day.hasCashInHand ? cashInHand - cashNeeded : 0.0;

            QJsonObject item;
            item.insert(QStringLiteral("date"), date.toString(Qt::ISODate));
            item.insert(QStringLiteral("opening_cash"), openingCash);
            item.insert(QStringLiteral("cash_in"), day.cashIn);
            item.insert(QStringLiteral("cash_expense"), day.cashExpense);
            item.insert(QStringLiteral("cash_needed"), cashNeeded);
            item.insert(QStringLiteral("cash_in_hand"), day.hasCashInHand ? QJsonValue(cashInHand) : QJsonValue(QJsonValue::Null));
            item.insert(QStringLiteral("cash_short_excess"), shortExcessVal);
            item.insert(QStringLiteral("bank"), day.bank);
            item.insert(QStringLiteral("credit_sale"), day.creditSale);
            item.insert(QStringLiteral("total_sales"), day.totalSales);
            rows.append(item);
            previousClosing = day.hasCashInHand ? cashInHand : cashNeeded;
        }
        return rows;
    }

    double openingCashBeforeDate(QSqlDatabase database, const QDate& startDate) const {
        const double seed = numericSetting(database, QStringLiteral("opening_cash_seed"), 0.0);
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(TRIM(txn_type)) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date < ?"
        ));
        query.addBindValue(startDate.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read opening cash before date"));
        query.next();
        return seed + query.value(0).toDouble() - query.value(1).toDouble();
    }

    QJsonArray transactionRowsToJson(QSqlQuery& query) const {
        QJsonArray rows;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), query.value(0).toInt());
            item.insert(QStringLiteral("date"), query.value(1).toString());
            item.insert(QStringLiteral("bill_no"), query.value(2).toString());
            item.insert(QStringLiteral("party"), query.value(3).toString());
            item.insert(QStringLiteral("type"), query.value(4).toString());
            item.insert(QStringLiteral("mode"), query.value(5).toString());
            item.insert(QStringLiteral("amount"), query.value(6).toDouble());
            rows.append(item);
        }
        return rows;
    }

    double receivables(QSqlDatabase database, const QPair<QDate, QDate>& bounds) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN UPPER(TRIM(t.txn_type))='SALE' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(TRIM(t.txn_type)) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END) "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE p.type IN ('Customer', 'Credit Customer') AND t.txn_date >= ? AND t.txn_date <= ?"
        ));
        query.addBindValue(bounds.first.toString(Qt::ISODate));
        query.addBindValue(bounds.second.toString(Qt::ISODate));
        executePrepared(query, QStringLiteral("Read receivables"));
        query.next();
        return query.value(0).toDouble() - query.value(1).toDouble();
    }
};


// ---------------------------------------------------------------------------
// InventoryService
// ---------------------------------------------------------------------------
class LiteInventoryService final : public domain::InventoryService, private SqliteServiceBase {
public:
    LiteInventoryService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray loadSnapshot(const QString& financialYear, int month) override {
        validateInventoryPeriod(financialYear, month);
        SqliteDb db = openDb();
        const QString company = companyScope(db.handle());

        QSqlQuery itemQuery(db.handle());
        itemQuery.prepare(QStringLiteral(
            "SELECT id, client_row_id, name, cost, min_stock FROM inventory_items WHERE company=? ORDER BY id"
        ));
        itemQuery.addBindValue(company);
        executePrepared(itemQuery, QStringLiteral("Load inventory items"));

        QJsonArray rows;
        while (itemQuery.next()) {
            const int itemId = itemQuery.value(0).toInt();
            QJsonObject item;
            item.insert(QStringLiteral("id"), itemId);
            item.insert(QStringLiteral("row_id"), itemQuery.value(1).toString());
            item.insert(QStringLiteral("name"), itemQuery.value(2).toString());
            item.insert(QStringLiteral("cost"), itemQuery.value(3).toDouble());
            item.insert(QStringLiteral("min_stock"), itemQuery.value(4).toDouble());
            applyDayValues(db.handle(), QStringLiteral("inventory_quantities"), itemId, company, financialYear, month, QStringLiteral("qty_"), item);
            applyDayValues(db.handle(), QStringLiteral("inventory_purchases"), itemId, company, financialYear, month, QStringLiteral("purchase_"), item);
            rows.append(item);
        }
        return rows;
    }


    void saveSnapshot(const QString& financialYear, int month, const QJsonArray& rows) override {
        validateInventoryPeriod(financialYear, month);
        SqliteDb db = openDb();
        const QString company = companyScope(db.handle());

        if (!db.handle().transaction()) {
            throw domain::DomainError(QStringLiteral("Could not start inventory save transaction: %1")
                .arg(db.handle().lastError().text()).toStdString());
        }
        try {
            for (const QJsonValue& value : rows) {
                const QJsonObject row = value.toObject();
                const QString name = row.value(QStringLiteral("name")).toString().trimmed();
                if (name.isEmpty()) continue;
                const QString rowId = row.value(QStringLiteral("row_id")).toString().trimmed().isEmpty()
                    ? QStringLiteral("inv-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
                    : row.value(QStringLiteral("row_id")).toString().trimmed();
                const int itemId = upsertItem(db.handle(), company, rowId, name,
                    row.value(QStringLiteral("cost")).toDouble(),
                    row.value(QStringLiteral("min_stock")).toDouble());
                replaceDayValues(db.handle(), QStringLiteral("inventory_quantities"), itemId, company, financialYear, month, QStringLiteral("qty_"), row);
                replaceDayValues(db.handle(), QStringLiteral("inventory_purchases"), itemId, company, financialYear, month, QStringLiteral("purchase_"), row);
            }
            writeAudit(db.handle(), QStringLiteral("native-admin"), QStringLiteral("Save Inventory"),
                QStringLiteral("Saved inventory snapshot ") + financialYear + QStringLiteral(" month ") + QString::number(month));
        } catch (...) {
            db.handle().rollback();
            throw;
        }
        if (!db.handle().commit()) {
            const QString detail = db.handle().lastError().text();
            db.handle().rollback();
            throw domain::DomainError(QStringLiteral("Could not commit inventory save transaction: %1").arg(detail).toStdString());
        }
    }


    QJsonArray stockValue(const QString& financialYear, int month) override {
        const QJsonArray rows = loadSnapshot(financialYear, month);
        QJsonArray values;
        for (int day = 1; day <= 31; day += 1) {
            double totalQuantity = 0.0;
            double totalValue = 0.0;
            const QString key = dayKey(QStringLiteral("qty_"), day);
            for (const QJsonValue& value : rows) {
                const QJsonObject row = value.toObject();
                const double quantity = row.value(key).toDouble();
                totalQuantity += quantity;
                totalValue += quantity * row.value(QStringLiteral("cost")).toDouble();
            }
            QJsonObject item;
            item.insert(QStringLiteral("day"), day);
            item.insert(QStringLiteral("quantity"), totalQuantity);
            item.insert(QStringLiteral("stock_value"), totalValue);
            values.append(item);
        }
        return values;
    }


    QByteArray buildPdfPreview(const domain::InventoryPdfMailRequest& request) override {
        if (request.rows.isEmpty()) {
            throw domain::DomainError("No inventory rows provided for PDF preview");
        }

        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly)) {
            throw domain::DomainError("Could not open memory buffer for inventory PDF");
        }

        QPdfWriter writer(&buffer);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Portrait);
        writer.setResolution(96);
        writer.setTitle(QStringLiteral("M-Finlogs Inventory Report"));

        QPainter painter(&writer);
        if (!painter.isActive()) {
            throw domain::DomainError("Could not start PDF painter for inventory report");
        }

        const QString fontFamily = QStringLiteral("Inter Tight");
        QFont titleFont(fontFamily, 16, QFont::Bold);
        QFont subtitleFont(fontFamily, 8);
        QFont headingFont(fontFamily, 7, QFont::Bold);
        QFont bodyFont(fontFamily, 7);
        QFont metricValueFont(fontFamily, 16, QFont::Bold);
        QFont metricLabelFont(fontFamily, 7);
        QFont chipFont(fontFamily, 9, QFont::Bold);

        const QColor ink(QStringLiteral("#111827"));
        const QColor muted(QStringLiteral("#6B7280"));
        const QColor headerBg(QStringLiteral("#F8FAFC"));
        const QColor headerFg(QStringLiteral("#374151"));
        const QColor gridColor(QStringLiteral("#E5E7EB"));
        const QColor altRowBg(QStringLiteral("#FAFAFA"));
        const QColor cardBg(QStringLiteral("#F8FAFC"));
        const QColor cardBorder(QStringLiteral("#CBD5E1"));
        const QColor chipBg(QStringLiteral("#1F2937"));
        const QColor currentDayBg(QStringLiteral("#DBEAFE"));
        const QColor currentDayFg(QStringLiteral("#1E3A8A"));
        const QColor currentDayBorder(QStringLiteral("#3B82F6"));
        const QColor reorderBg(QStringLiteral("#FFF1F2"));
        const QColor reorderFg(QStringLiteral("#DC2626"));
        const QColor groupBg(QStringLiteral("#E5E7EB"));
        const QColor groupFg(QStringLiteral("#64748B"));
        const QColor purchaseGreenFg(QStringLiteral("#059669"));

        const int pageWidth = 794;
        const int pageBottom = 1080;
        const int left = 28;
        const int right = pageWidth - 20;
        const int today = QDate::currentDate().day();


        int maxDays = 0;
        for (const auto& row : request.rows) {
            maxDays = qMax(maxDays, row.quantities.size());
            maxDays = qMax(maxDays, row.purchaseQuantities.size());
        }
        if (maxDays <= 0) maxDays = 31;

        const int endDay = qMin(today, maxDays);
        const int startDay = qMax(1, endDay - 6);
        QVector<int> days;
        for (int d = startDay; d <= endDay; ++d) days.append(d);

        struct ReportRow {
            QString name;
            double openingQty;
            double closingQty;
            double purchaseTotal;
            double cost;
            double minStock;
            bool isReorder;
            bool isGroup;
            QVector<double> dayQty;
            QVector<double> dayPurchase;
        };

        QVector<ReportRow> reportRows;
        double grandTotal = 0.0;
        double grandPurchaseIn = 0.0;
        double totalOutflow = 0.0;
        int outflowDaysUsed = 0;
        int reorderCount = 0;
        int groupCounter = 0;

        for (const auto& row : request.rows) {
            if (row.name.trimmed().isEmpty()) {
                groupCounter += 1;
                if (!request.onlyReorder) {
                    reportRows.append(ReportRow{QStringLiteral("Group %1").arg(groupCounter), 0, 0, 0, 0, 0, false, true, {}, {}});
                }
                continue;
            }
            const int closingIdx = qMin(qMax(endDay - 1, 0), row.quantities.size() - 1);
            const double closingDayQty = closingIdx >= 0 ? row.quantities[closingIdx] : 0.0;
            const double closingDayPurchase = (closingIdx >= 0 && closingIdx < row.purchaseQuantities.size()) ? row.purchaseQuantities[closingIdx] : 0.0;
            const double closingQty = closingDayQty + closingDayPurchase;
            const int openingIdx = qMax(startDay - 1, 0);
            const double openingQty = openingIdx < row.quantities.size() ? row.quantities[openingIdx] : 0.0;

            double purchaseTotal = 0.0;
            QVector<double> dayQty, dayPurchase;
            for (int d : days) {
                const int idx = d - 1;
                const double q = idx < row.quantities.size() ? row.quantities[idx] : 0.0;
                const double p = idx < row.purchaseQuantities.size() ? row.purchaseQuantities[idx] : 0.0;
                dayQty.append(q);
                dayPurchase.append(p);
                purchaseTotal += p;
            }
            for (int i = 1; i < row.quantities.size(); ++i) {
                totalOutflow += qMax(row.quantities[i - 1] - row.quantities[i], 0.0);
            }
            outflowDaysUsed = qMax(outflowDaysUsed, qMax(static_cast<int>(row.quantities.size()) - 1, 1));
            const bool isReorder = row.minStock > 0.0 && closingQty < row.minStock;
            if (isReorder) reorderCount += 1;
            grandTotal += closingQty;
            grandPurchaseIn += purchaseTotal;
            if (request.onlyReorder && !isReorder) continue;
            reportRows.append(ReportRow{row.name, openingQty, closingQty, purchaseTotal, row.cost, row.minStock, isReorder, false, dayQty, dayPurchase});
        }
        const double avgDaily = outflowDaysUsed > 0 ? totalOutflow / outflowDaysUsed : 0.0;


        auto fmt = [](double v, int decimals = 0) -> QString {
            if (std::abs(v) < 0.005) return QStringLiteral("0");
            return QString::number(v, 'f', decimals);
        };

        int y = 18;

        // Title
        painter.setFont(titleFont);
        painter.setPen(ink);
        painter.drawText(QRect(left, y, 420, 24), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("M-Finlogs Inventory Report"));

        // Month chip
        painter.setFont(chipFont);
        const int chipW = 200, chipH = 28;
        const int chipX = right - chipW;
        painter.fillRect(QRect(chipX, y - 2, chipW, chipH), chipBg);
        painter.setPen(Qt::white);
        painter.drawText(QRect(chipX, y - 2, chipW, chipH), Qt::AlignCenter,
                         QStringLiteral("MONTH: %1").arg(request.month.toUpper()));
        y += 36;

        const QString avgLabel = request.averageMode == QStringLiteral("last7")
            ? QStringLiteral("Last 7 Days") : QStringLiteral("Monthly Average");

        painter.setFont(subtitleFont);
        painter.setPen(muted);
        painter.drawText(QRect(left + 8, y, right - left, 14), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("%1 | Products: %2 | Current Qty: %3 | Days: %4-%5 | Rule: %6")
                .arg(request.financialYear).arg(reportRows.size()).arg(grandTotal, 0, 'f', 2).arg(startDay).arg(endDay).arg(avgLabel));
        y += 14;
        painter.drawText(QRect(left + 8, y, right - left, 14), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Generated: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("dd MMM yyyy, HH:mm"))));
        y += 20;

        // Metrics
        const int cardsH = 44;
        painter.fillRect(QRect(left, y, right - left, cardsH), cardBg);
        painter.setPen(cardBorder);
        painter.drawRect(QRect(left, y, right - left, cardsH));
        const int cardW = (right - left) / 4;

        auto drawMetric = [&](int idx, const QString& value, const QString& label) {
            const int x = left + idx * cardW;
            if (idx > 0) { painter.setPen(gridColor); painter.drawLine(x, y, x, y + cardsH); }
            painter.setPen(ink); painter.setFont(metricValueFont);
            painter.drawText(QRect(x + 10, y + 3, cardW - 18, 24), Qt::AlignLeft | Qt::AlignVCenter, value);
            painter.setPen(muted); painter.setFont(metricLabelFont);
            painter.drawText(QRect(x + 10, y + 27, cardW - 18, 13), Qt::AlignLeft | Qt::AlignVCenter, label);
        };
        drawMetric(0, QString::number(grandTotal, 'f', 2), QStringLiteral("Current Quantity"));
        drawMetric(1, QString::number(grandPurchaseIn, 'f', 2), QStringLiteral("Purchase In"));
        drawMetric(2, QString::number(avgDaily, 'f', 1), QStringLiteral("Avg Daily Movement"));
        drawMetric(3, QString::number(reorderCount), QStringLiteral("Reorder Products"));
        y += cardsH + 12;


        // Table
        painter.setFont(headingFont);
        painter.setPen(ink);
        painter.drawText(QRect(left + 8, y, 500, 14), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Daily Stock (Last 7 Days: %1-%2)").arg(startDay).arg(endDay));
        y += 18;

        const int rowH = 24;
        const int nameColW = 150;
        const int openColW = 64;
        const int purchColW = 76;
        const int closeColW = 64;
        const int fixedW = nameColW + openColW + purchColW + closeColW;
        const int dayColW = qMax(36, (right - left - fixedW) / qMax(days.size(), 1));

        auto drawGridVerticals = [&](int topY, int height) {
            int x = left;
            painter.setPen(gridColor);
            painter.drawLine(x, topY, x, topY + height);
            x += nameColW; painter.drawLine(x, topY, x, topY + height);
            x += openColW; painter.drawLine(x, topY, x, topY + height);
            x += purchColW; painter.drawLine(x, topY, x, topY + height);
            x += closeColW; painter.drawLine(x, topY, x, topY + height);
            for (int i = 0; i < days.size(); ++i) { x += dayColW; painter.drawLine(x, topY, x, topY + height); }
        };

        auto drawTableHeader = [&]() {
            painter.fillRect(QRect(left, y, right - left, rowH), headerBg);
            int x = left;
            painter.setFont(headingFont); painter.setPen(headerFg);
            painter.drawText(QRect(x + 6, y, nameColW - 8, rowH), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Product"));
            x += nameColW;
            painter.drawText(QRect(x, y, openColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Opening"));
            x += openColW;
            painter.drawText(QRect(x, y, purchColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Purchase"));
            x += purchColW;
            painter.drawText(QRect(x, y, closeColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Closing"));
            x += closeColW;
            for (int d : days) {
                const bool isCurrent = d == today;
                if (isCurrent) { painter.fillRect(QRect(x, y, dayColW, rowH), currentDayBg); painter.setPen(currentDayFg); }
                else { painter.setPen(headerFg); }
                painter.drawText(QRect(x, y, dayColW - 3, rowH), Qt::AlignRight | Qt::AlignVCenter, QString::number(d));
                x += dayColW;
            }
            drawGridVerticals(y, rowH);
            painter.setPen(gridColor);
            painter.drawLine(left, y, right, y);
            painter.drawLine(left, y + rowH, right, y + rowH);
            y += rowH;
        };

        drawTableHeader();


        for (int ri = 0; ri < reportRows.size(); ++ri) {
            if (y + rowH > pageBottom) { writer.newPage(); y = 24; drawTableHeader(); }
            const ReportRow& rr = reportRows[ri];
            if (rr.isGroup) {
                painter.fillRect(QRect(left, y, right - left, rowH - 2), groupBg);
                painter.setPen(groupFg); painter.setFont(headingFont);
                painter.drawText(QRect(left + 8, y, right - left - 16, rowH - 2), Qt::AlignLeft | Qt::AlignVCenter, rr.name);
                painter.setPen(gridColor); painter.drawLine(left, y + rowH - 2, right, y + rowH - 2);
                y += rowH - 2;
                continue;
            }
            if (ri % 2 == 1) painter.fillRect(QRect(left, y, right - left, rowH), altRowBg);
            if (rr.isReorder) painter.fillRect(QRect(left, y, right - left, rowH), reorderBg);

            int x = left;
            painter.setFont(rr.isReorder ? headingFont : bodyFont);
            painter.setPen(rr.isReorder ? reorderFg : ink);
            painter.drawText(QRect(x + 6, y, nameColW - 10, rowH), Qt::AlignLeft | Qt::AlignVCenter, rr.name.left(22));
            x += nameColW;
            painter.setFont(bodyFont);
            painter.setPen(rr.isReorder ? reorderFg : ink);
            painter.drawText(QRect(x, y, openColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, fmt(rr.openingQty));
            x += openColW;
            painter.drawText(QRect(x, y, purchColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, fmt(rr.purchaseTotal));
            x += purchColW;
            painter.drawText(QRect(x, y, closeColW - 6, rowH), Qt::AlignRight | Qt::AlignVCenter, fmt(rr.closingQty));
            x += closeColW;

            for (int di = 0; di < days.size(); ++di) {
                const int d = days[di];
                const bool isCurrent = d == today;
                if (isCurrent) {
                    painter.fillRect(QRect(x, y, dayColW, rowH), currentDayBg);
                    painter.setPen(currentDayBorder);
                    painter.drawLine(x, y, x, y + rowH);
                    painter.drawLine(x + dayColW - 1, y, x + dayColW - 1, y + rowH);
                }
                const double qty = di < rr.dayQty.size() ? rr.dayQty[di] : 0.0;
                const double purch = di < rr.dayPurchase.size() ? rr.dayPurchase[di] : 0.0;
                if (purch > 0.0) {
                    painter.setPen(isCurrent ? currentDayFg : (rr.isReorder ? reorderFg : ink));
                    painter.drawText(QRect(x, y + 1, dayColW - 5, rowH / 2), Qt::AlignRight | Qt::AlignBottom, fmt(qty));
                    painter.setPen(purchaseGreenFg);
                    painter.drawText(QRect(x, y + rowH / 2, dayColW - 5, rowH / 2), Qt::AlignRight | Qt::AlignTop, QStringLiteral("+%1").arg(fmt(purch)));
                } else {
                    painter.setPen(isCurrent ? currentDayFg : (rr.isReorder ? reorderFg : ink));
                    painter.drawText(QRect(x, y, dayColW - 5, rowH), Qt::AlignRight | Qt::AlignVCenter, fmt(qty));
                }
                x += dayColW;
            }
            drawGridVerticals(y, rowH);
            painter.setPen(gridColor);
            painter.drawLine(left, y + rowH, right, y + rowH);
            y += rowH;
        }
        painter.end();
        return bytes;
    }


    void sendPdfMail(const domain::InventoryPdfMailRequest& /*request*/) override {
        throw domain::DomainError("Email sending is not supported in local SQLite mode");
    }

private:
    void validateInventoryPeriod(const QString& financialYear, int month) const {
        if (financialYear.trimmed().isEmpty()) {
            throw domain::DomainError("Inventory financial year is required");
        }
        if (month < 1 || month > 12) {
            throw domain::DomainError(QStringLiteral("Inventory month must be 1-12: %1").arg(month).toStdString());
        }
    }

    QString companyScope(QSqlDatabase database) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
        query.addBindValue(QStringLiteral("company_name"));
        executePrepared(query, QStringLiteral("Read inventory company"));
        if (query.next() && !query.value(0).toString().trimmed().isEmpty()) {
            return normalizeKey(query.value(0).toString());
        }
        return QStringLiteral("default");
    }

    QString dayKey(const QString& prefix, int day) const {
        return QStringLiteral("%1%2").arg(prefix, QString::number(day).rightJustified(2, QLatin1Char('0')));
    }

    void applyDayValues(QSqlDatabase database, const QString& tableName, int itemId, const QString& company,
                        const QString& financialYear, int month, const QString& prefix, QJsonObject& item) const {
        QSqlQuery query(database);
        query.prepare(QStringLiteral("SELECT day, qty FROM %1 WHERE item_id=? AND company=? AND financial_year=? AND month=?").arg(tableName));
        query.addBindValue(itemId);
        query.addBindValue(company);
        query.addBindValue(financialYear);
        query.addBindValue(month);
        executePrepared(query, QStringLiteral("Load inventory day values"));
        while (query.next()) {
            const int day = query.value(0).toInt();
            if (day >= 1 && day <= 31) {
                item.insert(dayKey(prefix, day), query.value(1).toDouble());
            }
        }
    }


    int upsertItem(QSqlDatabase database, const QString& company, const QString& rowId, const QString& name, double cost, double minStock) const {
        QSqlQuery selectQuery(database);
        selectQuery.prepare(QStringLiteral("SELECT id FROM inventory_items WHERE company=? AND client_row_id=?"));
        selectQuery.addBindValue(company);
        selectQuery.addBindValue(rowId);
        executePrepared(selectQuery, QStringLiteral("Read inventory item by row id"));
        if (selectQuery.next()) {
            const int itemId = selectQuery.value(0).toInt();
            QSqlQuery updateQuery(database);
            updateQuery.prepare(QStringLiteral("UPDATE inventory_items SET name=?, cost=?, min_stock=?, updated_at=datetime('now') WHERE id=?"));
            updateQuery.addBindValue(name);
            updateQuery.addBindValue(cost);
            updateQuery.addBindValue(minStock);
            updateQuery.addBindValue(itemId);
            executePrepared(updateQuery, QStringLiteral("Update inventory item"));
            return itemId;
        }
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral("INSERT INTO inventory_items (client_row_id, company, name, cost, min_stock) VALUES (?, ?, ?, ?, ?)"));
        insertQuery.addBindValue(rowId);
        insertQuery.addBindValue(company);
        insertQuery.addBindValue(name);
        insertQuery.addBindValue(cost);
        insertQuery.addBindValue(minStock);
        executePrepared(insertQuery, QStringLiteral("Create inventory item"));
        return static_cast<int>(insertQuery.lastInsertId().toLongLong());
    }

    void replaceDayValues(QSqlDatabase database, const QString& tableName, int itemId, const QString& company,
                          const QString& financialYear, int month, const QString& prefix, const QJsonObject& row) const {
        QSqlQuery deleteQuery(database);
        deleteQuery.prepare(QStringLiteral("DELETE FROM %1 WHERE item_id=? AND company=? AND financial_year=? AND month=?").arg(tableName));
        deleteQuery.addBindValue(itemId);
        deleteQuery.addBindValue(company);
        deleteQuery.addBindValue(financialYear);
        deleteQuery.addBindValue(month);
        executePrepared(deleteQuery, QStringLiteral("Clear inventory day values"));

        for (int day = 1; day <= 31; day += 1) {
            const double quantity = row.value(dayKey(prefix, day)).toDouble();
            if (std::abs(quantity) < 0.005) continue;
            QSqlQuery insertQuery(database);
            insertQuery.prepare(QStringLiteral("INSERT INTO %1 (item_id, company, financial_year, month, day, qty) VALUES (?, ?, ?, ?, ?, ?)").arg(tableName));
            insertQuery.addBindValue(itemId);
            insertQuery.addBindValue(company);
            insertQuery.addBindValue(financialYear);
            insertQuery.addBindValue(month);
            insertQuery.addBindValue(day);
            insertQuery.addBindValue(quantity);
            executePrepared(insertQuery, QStringLiteral("Save inventory day value"));
        }
    }
};


// ---------------------------------------------------------------------------
// BackupService (file-copy based for SQLite)
// ---------------------------------------------------------------------------
class LiteBackupService final : public domain::BackupService, private SqliteServiceBase {
public:
    LiteBackupService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    domain::BackupResult backup(const QString& destinationPath) override {
        const QString requestedPath = destinationPath.trimmed().isEmpty()
            ? QFileInfo(dbPath()).absolutePath() + QStringLiteral("/backups")
            : destinationPath.trimmed();
        QDir directory(requestedPath);
        if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
            throw domain::DomainError(QStringLiteral("Could not create backup directory: %1").arg(requestedPath).toStdString());
        }

        // Checkpoint any WAL journal into the main .db so the single-file copy
        // is complete and consistent (otherwise recent writes can be missed).
        {
            SqliteDb db(dbPath());
            if (db.open()) {
                QSqlQuery cp(db.handle());
                cp.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
            }
        }

        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString baseName = QFileInfo(dbPath()).completeBaseName();
        const QString backupPath = directory.filePath(QStringLiteral("%1_%2.db").arg(baseName, timestamp));

        if (!QFile::copy(dbPath(), backupPath)) {
            throw domain::DomainError(QStringLiteral("Could not copy database to backup: %1").arg(backupPath).toStdString());
        }
        return domain::BackupResult{QStringLiteral("Backup complete"), backupPath, QString()};
    }

    domain::BackupResult autoBackup() override {
        return backup(QString());
    }

    void restore(const QString& backupPath) override {
        const QFileInfo backupFile(backupPath);
        if (!backupFile.exists() || !backupFile.isFile()) {
            throw domain::DomainError(QStringLiteral("Backup file not found: %1").arg(backupPath).toStdString());
        }

        const QString target = QFileInfo(dbPath()).absoluteFilePath();
        const QString source = backupFile.absoluteFilePath();

        // No-op if the chosen file is already the live database.
        if (QString::compare(source, target, Qt::CaseInsensitive) == 0) {
            return;
        }

        // SQLite in WAL mode keeps -wal/-shm sidecar files. They MUST be
        // removed when swapping the main db file, otherwise the restored
        // database is mixed with stale journal data (corruption / old data).
        const QStringList sidecars = {
            target,
            target + QStringLiteral("-wal"),
            target + QStringLiteral("-shm"),
            target + QStringLiteral("-journal")
        };
        for (const QString& f : sidecars) {
            if (QFile::exists(f) && !QFile::remove(f)) {
                throw domain::DomainError(QStringLiteral(
                    "Could not remove current database file for restore (is the app still using it?): %1").arg(f).toStdString());
            }
        }

        if (!QFile::copy(source, target)) {
            throw domain::DomainError(QStringLiteral("Could not copy backup to database path: %1 -> %2")
                .arg(source, target).toStdString());
        }
    }
};


// ---------------------------------------------------------------------------
// AuditService
// ---------------------------------------------------------------------------
class LiteAuditService final : public domain::AuditService, private SqliteServiceBase {
public:
    LiteAuditService(const QString& path, const std::shared_ptr<domain::ConfigService>& cfg)
        : SqliteServiceBase(path, cfg) {}

    QJsonArray listAuditLogs(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        SqliteDb db = openDb();
        QSqlQuery query(db.handle());
        if (!query.exec(QStringLiteral("SELECT timestamp, username, action, details FROM audit_logs ORDER BY timestamp DESC LIMIT 500"))) {
            throwSqlError(QStringLiteral("List audit logs"), query);
        }
        QJsonArray logs;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("timestamp"), query.value(0).toString());
            item.insert(QStringLiteral("username"), query.value(1).toString());
            item.insert(QStringLiteral("action"), query.value(2).toString());
            item.insert(QStringLiteral("details"), query.value(3).toString());
            logs.append(item);
        }
        return logs;
    }

    void writeAuditLog(const QString& username, const QString& action, const QString& details) override {
        SqliteDb db = openDb();
        writeAudit(db.handle(), username, action, details);
    }
};


// ---------------------------------------------------------------------------
// UpdateService (no-op stubs for local mode)
// ---------------------------------------------------------------------------
class LiteUpdateService final : public domain::UpdateService {
public:
    void checkForUpdates() override { }
    void restartAndInstall() override { }
};

} // namespace (anonymous)

// ---------------------------------------------------------------------------
// Factory function
// ---------------------------------------------------------------------------
domain::ServiceRegistry createSqliteBusinessServices(const QString& sqlitePath, const std::shared_ptr<domain::ConfigService>& configService) {
    domain::ServiceRegistry registry;
    registry.config = configService;
    registry.auth = std::make_shared<LiteAuthService>(sqlitePath, configService);
    registry.users = std::make_shared<LiteUserService>(sqlitePath, configService);
    registry.companies = std::make_shared<LiteCompanyService>(sqlitePath, configService);
    registry.financialYears = std::make_shared<LiteFinancialYearService>(sqlitePath, configService);
    registry.transactions = std::make_shared<LiteTransactionService>(sqlitePath, configService);
    registry.parties = std::make_shared<LitePartyService>(sqlitePath, configService);
    registry.reports = std::make_shared<LiteReportService>(sqlitePath, configService);
    registry.inventory = std::make_shared<LiteInventoryService>(sqlitePath, configService);
    registry.backups = std::make_shared<LiteBackupService>(sqlitePath, configService);
    registry.audit = std::make_shared<LiteAuditService>(sqlitePath, configService);
    registry.updates = std::make_shared<LiteUpdateService>();
    return registry;
}

} // namespace mfinlogs::data
