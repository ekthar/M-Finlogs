#include "data/SqlBusinessServices.h"

#include "data/SchemaInitializer.h"
#include "data/SqlDatabase.h"
#include "domain/DomainErrors.h"

#include <QCryptographicHash>
#include <QBuffer>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QHostInfo>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSslSocket>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTcpSocket>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <memory>

namespace mfinlogs::data {
namespace {

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
    case domain::TransactionType::Sale:
        return QStringLiteral("Sale");
    case domain::TransactionType::SaleReturn:
        return QStringLiteral("Sale Return");
    case domain::TransactionType::Purchase:
        return QStringLiteral("Purchase");
    case domain::TransactionType::Expense:
        return QStringLiteral("Expense");
    case domain::TransactionType::Receipt:
        return QStringLiteral("Receipt");
    }
    throw domain::DomainError("Unknown transaction type");
}

domain::TransactionType transactionTypeFromString(const QString& type) {
    const QString normalized = type.trimmed().toLower();
    if (normalized == QStringLiteral("sale")) {
        return domain::TransactionType::Sale;
    }
    if (normalized == QStringLiteral("sale return") || normalized == QStringLiteral("sales return") || normalized == QStringLiteral("return")) {
        return domain::TransactionType::SaleReturn;
    }
    if (normalized == QStringLiteral("purchase")) {
        return domain::TransactionType::Purchase;
    }
    if (normalized == QStringLiteral("expense")) {
        return domain::TransactionType::Expense;
    }
    if (normalized == QStringLiteral("receipt") || normalized == QStringLiteral("reciept")) {
        return domain::TransactionType::Receipt;
    }
    throw domain::DomainError(QStringLiteral("Unsupported transaction type: %1").arg(type).toStdString());
}

QString paymentModeToString(domain::PaymentMode mode) {
    switch (mode) {
    case domain::PaymentMode::Credit:
        return QStringLiteral("Credit");
    case domain::PaymentMode::Cash:
        return QStringLiteral("Cash");
    case domain::PaymentMode::Upi:
        return QStringLiteral("UPI");
    case domain::PaymentMode::Bank:
        return QStringLiteral("Bank");
    }
    throw domain::DomainError("Unknown payment mode");
}

QStringList mailRecipients(const QString& raw) {
    QStringList recipients;
    const QStringList parts = raw.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString recipient = part.trimmed();
        if (!recipient.isEmpty()) {
            recipients.append(recipient);
        }
    }
    return recipients;
}

void expectSmtpCode(QIODevice& socket, const QString& context, const QStringList& acceptedCodes) {
    if (!socket.waitForReadyRead(15000)) {
        throw domain::DomainError(QStringLiteral("SMTP timeout while waiting for %1").arg(context).toStdString());
    }

    QString response;
    while (socket.bytesAvailable() > 0 || socket.waitForReadyRead(200)) {
        response += QString::fromUtf8(socket.readAll());
        const QStringList lines = response.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
        if (!lines.isEmpty() && lines.last().size() >= 4 && lines.last().at(3) == QLatin1Char(' ')) {
            break;
        }
    }

    for (const QString& code : acceptedCodes) {
        if (response.startsWith(code)) {
            return;
        }
    }
    throw domain::DomainError(QStringLiteral("SMTP %1 failed. response=%2").arg(context, response.trimmed()).toStdString());
}

void writeSmtpLine(QIODevice& socket, const QString& command, const QString& context, const QStringList& acceptedCodes) {
    socket.write(command.toUtf8());
    socket.write("\r\n");
    if (!socket.waitForBytesWritten(15000)) {
        throw domain::DomainError(QStringLiteral("SMTP write timeout while sending %1").arg(context).toStdString());
    }
    expectSmtpCode(socket, context, acceptedCodes);
}

QString wrappedBase64(const QByteArray& bytes) {
    const QByteArray encoded = bytes.toBase64();
    QStringList lines;
    for (int index = 0; index < encoded.size(); index += 76) {
        lines.append(QString::fromLatin1(encoded.mid(index, 76)));
    }
    return lines.join(QStringLiteral("\r\n"));
}

domain::PaymentMode paymentModeFromString(const QString& mode) {
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("credit")) {
        return domain::PaymentMode::Credit;
    }
    if (normalized == QStringLiteral("cash")) {
        return domain::PaymentMode::Cash;
    }
    if (normalized == QStringLiteral("upi") || normalized == QStringLiteral("gpay") || normalized == QStringLiteral("google pay") || normalized == QStringLiteral("googlepay")) {
        return domain::PaymentMode::Upi;
    }
    if (normalized == QStringLiteral("bank")) {
        return domain::PaymentMode::Bank;
    }
    throw domain::DomainError(QStringLiteral("Unsupported payment mode: %1").arg(mode).toStdString());
}

QString sha256Hex(const QString& value) {
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString escapeSqlIdentifier(const QString& identifier) {
    return identifier.trimmed().replace(QStringLiteral("]"), QStringLiteral("]]"));
}

QString escapeSqlLiteral(const QString& value) {
    return value.trimmed().replace(QStringLiteral("'"), QStringLiteral("''"));
}

void throwSqlError(const QString& action, const QSqlQuery& query) {
    throw domain::DomainError(QStringLiteral("%1 failed: %2").arg(action, query.lastError().text()).toStdString());
}

void executePrepared(QSqlQuery& query, const QString& action) {
    if (!query.exec()) {
        throwSqlError(action, query);
    }
}

class SqlServiceBase {
public:
    explicit SqlServiceBase(std::shared_ptr<domain::ConfigService> configService)
        : configService_(std::move(configService)) {}

protected:
    domain::DatabaseConfig readConfig() const {
        return configService_->readDatabaseConfig();
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
            "IF EXISTS (SELECT 1 FROM app_settings WHERE setting_key=?) "
            "UPDATE app_settings SET setting_value=? WHERE setting_key=? "
            "ELSE INSERT INTO app_settings (setting_key, setting_value) VALUES (?, ?)"
        ));
        query.addBindValue(key);
        query.addBindValue(value);
        query.addBindValue(key);
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
        insertQuery.addBindValue(creditAllowed);
        executePrepared(insertQuery, QStringLiteral("Create party"));

        QSqlQuery idQuery(database);
        idQuery.prepare(QStringLiteral("SELECT party_id FROM parties WHERE normalized_name=?"));
        idQuery.addBindValue(normalized);
        executePrepared(idQuery, QStringLiteral("Read created party"));
        if (!idQuery.next()) {
            throw domain::DomainError(QStringLiteral("Created party was not found: %1").arg(name).toStdString());
        }
        return idQuery.value(0).toInt();
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
        QString company = QStringLiteral("default");
        QSqlQuery companyQuery(database);
        companyQuery.prepare(QStringLiteral("SELECT setting_value FROM app_settings WHERE setting_key=?"));
        companyQuery.addBindValue(QStringLiteral("company_name"));
        executePrepared(companyQuery, QStringLiteral("Read company for audit"));
        if (companyQuery.next() && !companyQuery.value(0).toString().isEmpty()) {
            company = companyQuery.value(0).toString();
        }

        QSqlQuery query(database);
        query.prepare(QStringLiteral("INSERT INTO audit_logs (username, action, details, company) VALUES (?, ?, ?, ?)"));
        query.addBindValue(username);
        query.addBindValue(action);
        query.addBindValue(details);
        query.addBindValue(company);
        executePrepared(query, QStringLiteral("Write audit log"));
    }

private:
    std::shared_ptr<domain::ConfigService> configService_;
};

class NativeAuthService final : public domain::AuthService, private SqlServiceBase {
public:
    explicit NativeAuthService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    bool setupRequired() const override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while checking setup status");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("SELECT password_hash FROM users WHERE username=?"));
        query.addBindValue(QStringLiteral("admin"));
        executePrepared(query, QStringLiteral("Read admin setup status"));
        return !query.next() || query.value(0).isNull() || query.value(0).toString().isEmpty();
    }

    domain::Session login(const QString& username, const QString& password) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while logging in");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("SELECT role, password_hash FROM users WHERE username=?"));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Read login user"));
        if (!query.next()) {
            throw domain::DomainError(QStringLiteral("Invalid credentials for user: %1").arg(username).toStdString());
        }

        const QString role = query.value(0).toString();
        const QString storedHash = query.value(1).toString();
        if (storedHash.isEmpty() || storedHash != sha256Hex(password)) {
            throw domain::DomainError(QStringLiteral("Invalid credentials for user: %1").arg(username).toStdString());
        }

        writeAudit(database.handle(), username, QStringLiteral("Login"), QStringLiteral("User logged in"));
        return domain::Session{username, roleFromString(role), QStringLiteral("native:%1:%2").arg(username, role)};
    }

    void setupAdmin(const QString& username, const QString& password) override {
        if (username.trimmed().isEmpty()) {
            throw domain::DomainError("Admin username is required");
        }
        if (password.size() < 8) {
            throw domain::DomainError("Admin password must be at least 8 characters");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while setting up admin");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "IF EXISTS (SELECT 1 FROM users WHERE username=?) "
            "UPDATE users SET password_hash=?, role=? WHERE username=? "
            "ELSE INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)"
        ));
        query.addBindValue(username);
        query.addBindValue(sha256Hex(password));
        query.addBindValue(QStringLiteral("admin"));
        query.addBindValue(username);
        query.addBindValue(username);
        query.addBindValue(sha256Hex(password));
        query.addBindValue(QStringLiteral("admin"));
        executePrepared(query, QStringLiteral("Setup admin"));
        writeAudit(database.handle(), QStringLiteral("system"), QStringLiteral("Setup"), QStringLiteral("Admin password configured"));
    }
};

class NativeUserService final : public domain::UserService, private SqlServiceBase {
public:
    explicit NativeUserService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray listUsers(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing users");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
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

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while creating user");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)"));
        query.addBindValue(username.trimmed());
        query.addBindValue(sha256Hex(password));
        query.addBindValue(roleToString(role));
        executePrepared(query, QStringLiteral("Create user"));
        writeAudit(database.handle(), QStringLiteral("native-admin"), QStringLiteral("Create User"), QStringLiteral("Created user ") + username);
    }

    void changePassword(const QString& username, const QString& password, const QString& adminToken) override {
        Q_UNUSED(adminToken);
        if (password.size() < 8) {
            throw domain::DomainError("Password must be at least 8 characters");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while changing password");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("UPDATE users SET password_hash=? WHERE username=?"));
        query.addBindValue(sha256Hex(password));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Change user password"));
        if (query.numRowsAffected() == 0) {
            throw domain::DomainError(QStringLiteral("User not found: %1").arg(username).toStdString());
        }
        writeAudit(database.handle(), QStringLiteral("native-admin"), QStringLiteral("Change Password"), QStringLiteral("Password changed for ") + username);
    }

    void deleteUser(const QString& username, const QString& adminToken) override {
        Q_UNUSED(adminToken);
        if (username == QStringLiteral("admin")) {
            throw domain::DomainError("Cannot delete default admin");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while deleting user");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("DELETE FROM users WHERE username=?"));
        query.addBindValue(username);
        executePrepared(query, QStringLiteral("Delete user"));
        writeAudit(database.handle(), QStringLiteral("native-admin"), QStringLiteral("Delete User"), QStringLiteral("Deleted user ") + username);
    }
};

class NativeCompanyService final : public domain::CompanyService, private SqlServiceBase {
public:
    explicit NativeCompanyService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray listCompanies() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing companies");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
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

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while selecting company");
        }
        SchemaInitializer(database.handle()).initialize(trimmedName);
        setTextSetting(database.handle(), QStringLiteral("company_name"), trimmedName);
    }
};

class NativeFinancialYearService final : public domain::FinancialYearService, private SqlServiceBase {
public:
    explicit NativeFinancialYearService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray listFinancialYears() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing financial years");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QStringList merged;
        const QString selected = SqlServiceBase::selectedFinancialYear(database.handle());
        merged.append(selected);
        merged.append(financialYearForDate(QDate::currentDate()));

        QSqlQuery query(database.handle());
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
        financialYearBounds(selected);

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while selecting financial year");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));
        setTextSetting(database.handle(), QStringLiteral("active_financial_year"), selected);
    }

    QString selectedFinancialYear() const override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading selected financial year");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));
        return SqlServiceBase::selectedFinancialYear(database.handle());
    }
};

class NativePartyService final : public domain::PartyService, private SqlServiceBase {
public:
    explicit NativePartyService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray listParties() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing parties");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
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
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while creating party");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        if (type == QStringLiteral("Bank")) {
            QSqlQuery countQuery(database.handle());
            if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM parties WHERE type = 'Bank'"))) {
                throwSqlError(QStringLiteral("Count bank parties"), countQuery);
            }
            countQuery.next();
            if (countQuery.value(0).toInt() > 0) {
                throw domain::DomainError("Only one Bank account is allowed");
            }
        }

        getOrCreatePartyId(database.handle(), name, type, creditAllowed);
    }

    void renameParty(const QString& oldName, const QString& newName, const QString& adminUser) override {
        const QString normalizedOldName = normalizeKey(oldName);
        const QString normalizedNewName = normalizeKey(newName);
        if (normalizedOldName.isEmpty() || normalizedNewName.isEmpty()) {
            throw domain::DomainError("Old and new party names are required");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while renaming party");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("UPDATE parties SET name=?, normalized_name=? WHERE normalized_name=?"));
        query.addBindValue(newName.trimmed());
        query.addBindValue(normalizedNewName);
        query.addBindValue(normalizedOldName);
        executePrepared(query, QStringLiteral("Rename party"));
        if (query.numRowsAffected() == 0) {
            throw domain::DomainError(QStringLiteral("Party not found: %1").arg(oldName).toStdString());
        }
        writeAudit(database.handle(), adminUser, QStringLiteral("Rename Party"), QStringLiteral("Renamed party from ") + oldName + QStringLiteral(" to ") + newName);
    }
};

class NativeTransactionService final : public domain::TransactionService, private SqlServiceBase {
public:
    explicit NativeTransactionService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    int addTransaction(const domain::TransactionCreateRequest& request) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while adding transaction");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const int partyId = getExistingPartyId(database.handle(), request.party);
        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount) "
            "OUTPUT INSERTED.txn_id VALUES (?, ?, ?, ?, ?, ?, ?)"
        ));
        query.addBindValue(request.date);
        query.addBindValue(request.billNo);
        query.addBindValue(partyId);
        query.addBindValue(transactionTypeToString(request.type));
        query.addBindValue(paymentModeToString(request.mode));
        query.addBindValue(financialYearForDate(request.date));
        query.addBindValue(request.amount);
        executePrepared(query, QStringLiteral("Add transaction"));
        if (!query.next()) {
            throw domain::DomainError("Transaction insert did not return an id");
        }
        return query.value(0).toInt();
    }

    QVector<domain::TransactionRow> listTransactions(int page, int limit, int days) override {
        if (page < 1 || limit < 1) {
            throw domain::DomainError("Transaction page and limit must be positive");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing transactions");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QString financialYear = selectedFinancialYear(database.handle());
        const QPair<QDate, QDate> bounds = financialYearBounds(financialYear);
        QDate lowerBound = bounds.first;
        if (days > 0) {
            lowerBound = std::max(lowerBound, QDate::currentDate().addDays(-days));
        }
        const int offset = (page - 1) * limit;

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount "
            "FROM transactions t "
            "JOIN parties p ON t.party_id = p.party_id "
            "WHERE t.txn_date >= ? AND t.txn_date <= ? "
            "ORDER BY t.txn_date DESC, t.txn_id DESC "
            "OFFSET ? ROWS FETCH NEXT ? ROWS ONLY"
        ));
        query.addBindValue(lowerBound);
        query.addBindValue(bounds.second);
        query.addBindValue(offset);
        query.addBindValue(limit);
        executePrepared(query, QStringLiteral("List transactions"));

        QVector<domain::TransactionRow> rows;
        while (query.next()) {
            rows.append(domain::TransactionRow{
                query.value(0).toInt(),
                query.value(1).toDate(),
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
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading transaction");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
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
            query.value(1).toDate(),
            query.value(2).toString(),
            query.value(3).toString(),
            transactionTypeFromString(query.value(4).toString()),
            paymentModeFromString(query.value(5).toString()),
            query.value(6).toDouble()
        };
    }

    void editTransaction(const domain::TransactionEditRequest& request) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while editing transaction");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QString field = request.field.trimmed();
        QString sql;
        QVariant value;
        if (field == QStringLiteral("txn_date")) {
            const QDate date = QDate::fromString(request.newValue, Qt::ISODate);
            if (!date.isValid()) {
                throw domain::DomainError(QStringLiteral("Invalid transaction date: %1").arg(request.newValue).toStdString());
            }
            sql = QStringLiteral("UPDATE transactions SET txn_date=?, financial_year=? WHERE txn_id=?");
            QSqlQuery query(database.handle());
            query.prepare(sql);
            query.addBindValue(date);
            query.addBindValue(financialYearForDate(date));
            query.addBindValue(request.transactionId);
            executePrepared(query, QStringLiteral("Edit transaction date"));
        } else if (field == QStringLiteral("party")) {
            const int partyId = getExistingPartyId(database.handle(), request.newValue);
            QSqlQuery query(database.handle());
            query.prepare(QStringLiteral("UPDATE transactions SET party_id=? WHERE txn_id=?"));
            query.addBindValue(partyId);
            query.addBindValue(request.transactionId);
            executePrepared(query, QStringLiteral("Edit transaction party"));
        } else {
            static const QHash<QString, QString> fields = {
                {QStringLiteral("bill_no"), QStringLiteral("bill_no")},
                {QStringLiteral("txn_type"), QStringLiteral("txn_type")},
                {QStringLiteral("payment_mode"), QStringLiteral("payment_mode")},
                {QStringLiteral("amount"), QStringLiteral("amount")}
            };
            if (!fields.contains(field)) {
                throw domain::DomainError(QStringLiteral("Invalid transaction field: %1").arg(field).toStdString());
            }
            QSqlQuery query(database.handle());
            query.prepare(QStringLiteral("UPDATE transactions SET %1=? WHERE txn_id=?").arg(fields.value(field)));
            query.addBindValue(request.newValue);
            query.addBindValue(request.transactionId);
            executePrepared(query, QStringLiteral("Edit transaction"));
        }
        writeAudit(database.handle(), request.adminUser, QStringLiteral("Edit Transaction"), QStringLiteral("Edited transaction ") + QString::number(request.transactionId));
    }

    void deleteTransaction(const domain::TransactionDeleteRequest& request) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while deleting transaction");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("DELETE FROM transactions WHERE txn_id=?"));
        query.addBindValue(request.transactionId);
        executePrepared(query, QStringLiteral("Delete transaction"));
        if (query.numRowsAffected() == 0) {
            throw domain::DomainError(QStringLiteral("Transaction not found: %1").arg(request.transactionId).toStdString());
        }
        writeAudit(database.handle(), request.adminUser, QStringLiteral("Delete Transaction"), QStringLiteral("Deleted transaction ") + QString::number(request.transactionId));
    }
};

class NativeReportService final : public domain::ReportService, private SqlServiceBase {
public:
    explicit NativeReportService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonObject dashboardMetrics() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading dashboard metrics");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(database.handle()));
        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' AND txn_date = CAST(GETDATE() AS DATE) THEN amount WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE RETURN','SALES RETURN','RETURN') AND txn_date = CAST(GETDATE() AS DATE) THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' AND MONTH(txn_date)=MONTH(GETDATE()) AND YEAR(txn_date)=YEAR(GETDATE()) THEN amount WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE RETURN','SALES RETURN','RETURN') AND MONTH(txn_date)=MONTH(GETDATE()) AND YEAR(txn_date)=YEAR(GETDATE()) THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode='Cash' AND UPPER(LTRIM(RTRIM(txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE','RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN payment_mode IN ('Bank','UPI','GPay','GPAY','Google Pay','GooglePay') AND UPPER(LTRIM(RTRIM(txn_type))) IN ('EXPENSE','SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        query.addBindValue(bounds.first);
        query.addBindValue(bounds.second);
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
        result.insert(QStringLiteral("receivables"), receivables(database.handle(), bounds));
        return result;
    }

    QJsonObject ledger(const QString& party, const domain::ReportRange& range) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading ledger");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery partyQuery(database.handle());
        partyQuery.prepare(QStringLiteral("SELECT party_id FROM parties WHERE normalized_name=?"));
        partyQuery.addBindValue(normalizeKey(party));
        executePrepared(partyQuery, QStringLiteral("Read ledger party"));

        QJsonObject result;
        QJsonArray rows;
        result.insert(QStringLiteral("opening_balance"), 0.0);
        result.insert(QStringLiteral("period_start"), range.start.toString(Qt::ISODate));
        result.insert(QStringLiteral("period_end"), range.end.toString(Qt::ISODate));
        result.insert(QStringLiteral("financial_year"), selectedFinancialYear(database.handle()));
        if (!partyQuery.next()) {
            result.insert(QStringLiteral("data"), rows);
            return result;
        }

        const int partyId = partyQuery.value(0).toInt();
        double openingBalance = 0.0;
        if (range.start.isValid()) {
            QSqlQuery openingQuery(database.handle());
            openingQuery.prepare(QStringLiteral(
                "SELECT SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' THEN amount "
                "WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN -amount ELSE 0 END) "
                "FROM transactions WHERE party_id=? AND txn_date < ?"
            ));
            openingQuery.addBindValue(partyId);
            openingQuery.addBindValue(range.start);
            executePrepared(openingQuery, QStringLiteral("Read ledger opening balance"));
            openingQuery.next();
            openingBalance = openingQuery.value(0).toDouble();
        }

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT txn_id, txn_date, bill_no, txn_type, payment_mode, amount "
            "FROM transactions WHERE party_id=? AND txn_date >= ? AND txn_date <= ? ORDER BY txn_date, txn_id"
        ));
        query.addBindValue(partyId);
        query.addBindValue(range.start);
        query.addBindValue(range.end);
        executePrepared(query, QStringLiteral("Read ledger rows"));

        double balance = openingBalance;
        while (query.next()) {
            const QString type = query.value(3).toString().trimmed().toUpper();
            const double amount = query.value(5).toDouble();
            if (type == QStringLiteral("SALE")) {
                balance += amount;
            } else if (type == QStringLiteral("RECEIPT") || type == QStringLiteral("RECIEPT") || type == QStringLiteral("SALE RETURN") || type == QStringLiteral("SALES RETURN") || type == QStringLiteral("RETURN")) {
                balance -= amount;
            }

            QJsonObject item;
            item.insert(QStringLiteral("id"), query.value(0).toInt());
            item.insert(QStringLiteral("date"), query.value(1).toDate().toString(Qt::ISODate));
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
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading day book");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT t.txn_id, t.txn_date, t.bill_no, p.name, t.txn_type, t.payment_mode, t.amount "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE t.txn_date=? ORDER BY t.txn_id"
        ));
        query.addBindValue(date);
        executePrepared(query, QStringLiteral("Read day book"));
        return transactionRowsToJson(query);
    }

    QJsonArray dailySummary(const domain::ReportRange& range) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading daily summary");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT txn_date, "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type)))='SALE' THEN amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('SALE RETURN','SALES RETURN','RETURN') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type))) IN ('RECEIPT','RECIEPT') THEN amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(txn_type)))='EXPENSE' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ? GROUP BY txn_date ORDER BY txn_date"
        ));
        query.addBindValue(range.start);
        query.addBindValue(range.end);
        executePrepared(query, QStringLiteral("Read daily summary"));

        QJsonArray rows;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("date"), query.value(0).toDate().toString(Qt::ISODate));
            item.insert(QStringLiteral("sales"), query.value(1).toDouble());
            item.insert(QStringLiteral("sale_returns"), query.value(2).toDouble());
            item.insert(QStringLiteral("receipts"), query.value(3).toDouble());
            item.insert(QStringLiteral("expenses"), query.value(4).toDouble());
            item.insert(QStringLiteral("net_sales"), query.value(1).toDouble() - query.value(2).toDouble());
            rows.append(item);
        }
        return rows;
    }

    QJsonArray shortExcess(const domain::ReportRange& range) override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading short/excess");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT cash_date, cash_in_hand FROM daily_cash WHERE cash_date >= ? AND cash_date <= ? ORDER BY cash_date"
        ));
        query.addBindValue(range.start);
        query.addBindValue(range.end);
        executePrepared(query, QStringLiteral("Read short/excess"));

        QJsonArray rows;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("date"), query.value(0).toDate().toString(Qt::ISODate));
            item.insert(QStringLiteral("cash_in_hand"), query.value(1).toDouble());
            rows.append(item);
        }
        return rows;
    }

    void saveCashInHand(const QDate& date, double amount) override {
        if (!date.isValid()) {
            throw domain::DomainError("Cash date is required");
        }
        if (amount < 0.0) {
            throw domain::DomainError("Cash in hand cannot be negative");
        }

        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while saving cash in hand");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "IF EXISTS (SELECT 1 FROM daily_cash WHERE cash_date=?) "
            "UPDATE daily_cash SET cash_in_hand=?, updated_at=GETDATE() WHERE cash_date=? "
            "ELSE INSERT INTO daily_cash (cash_date, cash_in_hand) VALUES (?, ?)"
        ));
        query.addBindValue(date);
        query.addBindValue(amount);
        query.addBindValue(date);
        query.addBindValue(date);
        query.addBindValue(amount);
        executePrepared(query, QStringLiteral("Save cash in hand"));
    }

    QJsonObject outstanding() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading outstanding report");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(database.handle()));
        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT p.name, p.type, "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type)))='SALE' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END) "
            "FROM parties p LEFT JOIN transactions t ON t.party_id=p.party_id AND t.txn_date >= ? AND t.txn_date <= ? "
            "WHERE p.type IN ('Customer','Credit Customer') GROUP BY p.name, p.type ORDER BY p.name"
        ));
        query.addBindValue(bounds.first);
        query.addBindValue(bounds.second);
        executePrepared(query, QStringLiteral("Read outstanding report"));

        QJsonArray rows;
        double total = 0.0;
        while (query.next()) {
            const double balance = query.value(2).toDouble() - query.value(3).toDouble();
            if (std::abs(balance) < 0.005) {
                continue;
            }
            QJsonObject item;
            item.insert(QStringLiteral("party"), query.value(0).toString());
            item.insert(QStringLiteral("type"), query.value(1).toString());
            item.insert(QStringLiteral("balance"), balance);
            rows.append(item);
            total += balance;
        }

        QJsonObject result;
        result.insert(QStringLiteral("data"), rows);
        result.insert(QStringLiteral("total"), total);
        return result;
    }

    QJsonArray trialBalance() override {
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading trial balance");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(database.handle()));

        QSqlQuery modeQuery(database.handle());
        modeQuery.prepare(QStringLiteral(
            "SELECT payment_mode, txn_type, SUM(amount) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ? GROUP BY payment_mode, txn_type"
        ));
        modeQuery.addBindValue(bounds.first);
        modeQuery.addBindValue(bounds.second);
        executePrepared(modeQuery, QStringLiteral("Read trial balance mode totals"));

        double cashIn = 0.0;
        double cashOut = 0.0;
        double bankIn = 0.0;
        double bankOut = 0.0;
        double upiIn = 0.0;
        double upiOut = 0.0;
        while (modeQuery.next()) {
            const QString mode = modeQuery.value(0).toString().trimmed().toLower();
            const QString type = modeQuery.value(1).toString().trimmed().toLower();
            const double amount = modeQuery.value(2).toDouble();
            double* inBucket = &cashIn;
            double* outBucket = &cashOut;
            if (mode == QStringLiteral("bank") || mode == QStringLiteral("gpay") || mode == QStringLiteral("google pay") || mode == QStringLiteral("googlepay")) {
                inBucket = &bankIn;
                outBucket = &bankOut;
            } else if (mode == QStringLiteral("upi")) {
                inBucket = &upiIn;
                outBucket = &upiOut;
            }
            if (type == QStringLiteral("sale") || type == QStringLiteral("receipt") || type == QStringLiteral("reciept")) {
                *inBucket += amount;
            } else if (type == QStringLiteral("expense") || type == QStringLiteral("sale return") || type == QStringLiteral("sales return") || type == QStringLiteral("return")) {
                *outBucket += amount;
            }
        }

        QSqlQuery debtorQuery(database.handle());
        debtorQuery.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN t.txn_type IN ('Receipt','Reciept','Sale Return','Sales Return','Return') THEN t.amount ELSE 0 END) "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE p.type='Credit Customer' AND t.txn_date >= ? AND t.txn_date <= ?"
        ));
        debtorQuery.addBindValue(bounds.first);
        debtorQuery.addBindValue(bounds.second);
        executePrepared(debtorQuery, QStringLiteral("Read trial balance debtors"));
        debtorQuery.next();
        const double debtors = debtorQuery.value(0).toDouble() - debtorQuery.value(1).toDouble();

        QSqlQuery profitQuery(database.handle());
        profitQuery.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return','Sales Return','Return') THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        profitQuery.addBindValue(bounds.first);
        profitQuery.addBindValue(bounds.second);
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
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while reading profit and loss");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QPair<QDate, QDate> bounds = financialYearBounds(selectedFinancialYear(database.handle()));
        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral(
            "SELECT "
            "SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type IN ('Sale Return','Sales Return','Return') THEN -amount ELSE 0 END), "
            "SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) "
            "FROM transactions WHERE txn_date >= ? AND txn_date <= ?"
        ));
        query.addBindValue(bounds.first);
        query.addBindValue(bounds.second);
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
    QJsonArray transactionRowsToJson(QSqlQuery& query) const {
        QJsonArray rows;
        while (query.next()) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), query.value(0).toInt());
            item.insert(QStringLiteral("date"), query.value(1).toDate().toString(Qt::ISODate));
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
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type)))='SALE' THEN t.amount ELSE 0 END), "
            "SUM(CASE WHEN UPPER(LTRIM(RTRIM(t.txn_type))) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN t.amount ELSE 0 END) "
            "FROM transactions t JOIN parties p ON t.party_id=p.party_id "
            "WHERE p.type IN ('Customer', 'Credit Customer') AND t.txn_date >= ? AND t.txn_date <= ?"
        ));
        query.addBindValue(bounds.first);
        query.addBindValue(bounds.second);
        executePrepared(query, QStringLiteral("Read receivables"));
        query.next();
        return query.value(0).toDouble() - query.value(1).toDouble();
    }
};

class NativeInventoryService final : public domain::InventoryService, private SqlServiceBase {
public:
    explicit NativeInventoryService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray loadSnapshot(const QString& financialYear, int month) override {
        validateInventoryPeriod(financialYear, month);
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while loading inventory");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QString company = companyScope(database.handle());
        QSqlQuery itemQuery(database.handle());
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
            applyDayValues(database.handle(), QStringLiteral("inventory_quantities"), itemId, company, financialYear, month, QStringLiteral("qty_"), item);
            applyDayValues(database.handle(), QStringLiteral("inventory_purchases"), itemId, company, financialYear, month, QStringLiteral("purchase_"), item);
            rows.append(item);
        }
        return rows;
    }

    void saveSnapshot(const QString& financialYear, int month, const QJsonArray& rows) override {
        validateInventoryPeriod(financialYear, month);
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while saving inventory");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        const QString company = companyScope(database.handle());
        for (const QJsonValue& value : rows) {
            const QJsonObject row = value.toObject();
            const QString name = row.value(QStringLiteral("name")).toString().trimmed();
            if (name.isEmpty()) {
                continue;
            }
            const QString rowId = row.value(QStringLiteral("row_id")).toString().trimmed().isEmpty()
                ? QStringLiteral("inv-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
                : row.value(QStringLiteral("row_id")).toString().trimmed();
            const int itemId = upsertItem(database.handle(), company, rowId, name, row.value(QStringLiteral("cost")).toDouble(), row.value(QStringLiteral("min_stock")).toDouble());
            replaceDayValues(database.handle(), QStringLiteral("inventory_quantities"), itemId, company, financialYear, month, QStringLiteral("qty_"), row);
            replaceDayValues(database.handle(), QStringLiteral("inventory_purchases"), itemId, company, financialYear, month, QStringLiteral("purchase_"), row);
        }
        writeAudit(database.handle(), QStringLiteral("native-admin"), QStringLiteral("Save Inventory"), QStringLiteral("Saved inventory snapshot ") + financialYear + QStringLiteral(" month ") + QString::number(month));
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
        writer.setResolution(96);
        writer.setTitle(QStringLiteral("M-Finlogs Inventory Report"));

        QPainter painter(&writer);
        if (!painter.isActive()) {
            throw domain::DomainError("Could not start PDF painter for inventory report");
        }

        QFont titleFont(QStringLiteral("Inter Tight"), 20, QFont::Bold);
        QFont headingFont(QStringLiteral("Inter Tight"), 10, QFont::Bold);
        QFont bodyFont(QStringLiteral("Inter Tight"), 9);
        const QColor ink(QStringLiteral("#24342e"));
        const QColor muted(QStringLiteral("#667268"));
        const QColor border(QStringLiteral("#d8cdb7"));
        const QColor band(QStringLiteral("#f5efe3"));
        const int left = 44;
        const int right = 750;
        painter.setFont(titleFont);
        painter.setPen(ink);
        painter.fillRect(QRect(0, 0, 794, 96), QColor(QStringLiteral("#fbf5e8")));
        painter.drawText(QRect(left, 28, 500, 34), Qt::AlignLeft, QStringLiteral("M-Finlogs Inventory Report"));
        painter.setFont(bodyFont);
        painter.setPen(muted);
        painter.drawText(QRect(left, 64, 700, 24), Qt::AlignLeft, QStringLiteral("%1 | %2 | %3").arg(request.financialYear, request.month, request.averageMode));
        if (!request.notes.trimmed().isEmpty()) {
            painter.drawText(QRect(left, 88, 700, 32), Qt::AlignLeft | Qt::TextWordWrap, request.notes.trimmed());
        }

        int y = request.notes.trimmed().isEmpty() ? 124 : 150;
        auto drawHeader = [&]() {
            painter.fillRect(QRect(left, y - 16, right - left, 26), band);
            painter.setPen(border);
            painter.drawLine(left, y + 12, right, y + 12);
            painter.setPen(ink);
            painter.setFont(headingFont);
            painter.drawText(left + 8, y, QStringLiteral("Product"));
            painter.drawText(330, y, QStringLiteral("Qty"));
            painter.drawText(420, y, QStringLiteral("Purchase"));
            painter.drawText(530, y, QStringLiteral("Cost"));
            if (request.includeStockValue) {
                painter.drawText(620, y, QStringLiteral("Value"));
            }
            painter.setFont(bodyFont);
            y += 34;
        };
        drawHeader();

        double grandValue = 0.0;
        double grandQuantity = 0.0;
        double grandPurchase = 0.0;
        int rowNumber = 0;
        for (const domain::InventoryProductRow& row : request.rows) {
            if (row.name.trimmed().isEmpty()) {
                continue;
            }
            double quantity = 0.0;
            for (double value : row.quantities) {
                quantity += value;
            }
            double purchaseQuantity = 0.0;
            for (double value : row.purchaseQuantities) {
                purchaseQuantity += value;
            }
            if (request.onlyReorder && quantity >= row.minStock) {
                continue;
            }
            const double stockValue = quantity * row.cost;
            grandQuantity += quantity;
            grandPurchase += purchaseQuantity;
            grandValue += stockValue;
            if (rowNumber % 2 == 1) {
                painter.fillRect(QRect(left, y - 14, right - left, 24), QColor(QStringLiteral("#fbfaf6")));
            }
            painter.setPen(ink);
            painter.drawText(left + 8, y, row.name.left(42));
            painter.drawText(330, y, QString::number(quantity, 'f', 2));
            painter.drawText(420, y, QString::number(purchaseQuantity, 'f', 2));
            painter.drawText(530, y, (QString(QChar(0x20B9)) + QStringLiteral("%1")).arg(row.cost, 0, 'f', 2));
            if (request.includeStockValue) {
                painter.drawText(620, y, (QString(QChar(0x20B9)) + QStringLiteral("%1")).arg(stockValue, 0, 'f', 2));
            }
            rowNumber += 1;
            y += 24;
            if (y > 1040) {
                writer.newPage();
                y = 70;
                drawHeader();
            }
        }
        y += 8;
        painter.setPen(border);
        painter.drawLine(left, y, right, y);
        y += 24;
        painter.setFont(headingFont);
        painter.setPen(ink);
        painter.drawText(left + 8, y, QStringLiteral("Totals"));
        painter.drawText(330, y, QString::number(grandQuantity, 'f', 2));
        painter.drawText(420, y, QString::number(grandPurchase, 'f', 2));
        if (request.includeStockValue) {
            painter.drawText(620, y, (QString(QChar(0x20B9)) + QStringLiteral("%1")).arg(grandValue, 0, 'f', 2));
        }
        painter.end();
        return bytes;
    }

    void sendPdfMail(const domain::InventoryPdfMailRequest& request) override {
        const QStringList toRecipients = mailRecipients(request.toEmail);
        const QStringList ccRecipients = mailRecipients(request.ccEmail);
        if (toRecipients.isEmpty()) {
            throw domain::DomainError("Inventory email requires at least one recipient");
        }
        if (request.senderEmail.trimmed().isEmpty() || request.senderPassword.isEmpty()) {
            throw domain::DomainError("Inventory email requires sender email and app password");
        }
        if (request.smtpHost.trimmed().isEmpty() || request.smtpPort <= 0) {
            throw domain::DomainError("Inventory email requires SMTP host and port");
        }

        const QByteArray pdf = buildPdfPreview(request);
        const QString boundary = QStringLiteral("mfinlogs-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        QString message;
        message += QStringLiteral("From: %1\r\n").arg(request.senderEmail.trimmed());
        message += QStringLiteral("To: %1\r\n").arg(toRecipients.join(QStringLiteral(", ")));
        if (!ccRecipients.isEmpty()) {
            message += QStringLiteral("Cc: %1\r\n").arg(ccRecipients.join(QStringLiteral(", ")));
        }
        message += QStringLiteral("Subject: %1\r\n").arg(request.subject.trimmed().isEmpty() ? QStringLiteral("Inventory Report") : request.subject.trimmed());
        message += QStringLiteral("MIME-Version: 1.0\r\n");
        message += QStringLiteral("Content-Type: multipart/mixed; boundary=\"%1\"\r\n\r\n").arg(boundary);
        message += QStringLiteral("--%1\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n%2\r\n\r\n")
            .arg(boundary, request.notes.trimmed().isEmpty() ? QStringLiteral("Inventory report attached.") : request.notes.trimmed());
        message += QStringLiteral("--%1\r\nContent-Type: application/pdf; name=\"inventory-report.pdf\"\r\nContent-Transfer-Encoding: base64\r\nContent-Disposition: attachment; filename=\"inventory-report.pdf\"\r\n\r\n%2\r\n--%1--\r\n")
            .arg(boundary, wrappedBase64(pdf));

        QSslSocket socket;
        socket.connectToHost(request.smtpHost.trimmed(), static_cast<quint16>(request.smtpPort));
        if (!socket.waitForConnected(15000)) {
            throw domain::DomainError(QStringLiteral("SMTP connect failed. host=%1 port=%2 error=%3")
                .arg(request.smtpHost, QString::number(request.smtpPort), socket.errorString()).toStdString());
        }
        expectSmtpCode(socket, QStringLiteral("greeting"), QStringList{QStringLiteral("220")});
        const QString localName = QHostInfo::localHostName().isEmpty() ? QStringLiteral("localhost") : QHostInfo::localHostName();
        writeSmtpLine(socket, QStringLiteral("EHLO %1").arg(localName), QStringLiteral("EHLO"), QStringList{QStringLiteral("250")});
        writeSmtpLine(socket, QStringLiteral("STARTTLS"), QStringLiteral("STARTTLS"), QStringList{QStringLiteral("220")});
        socket.startClientEncryption();
        if (!socket.waitForEncrypted(15000)) {
            throw domain::DomainError(QStringLiteral("SMTP TLS handshake failed. error=%1").arg(socket.errorString()).toStdString());
        }
        writeSmtpLine(socket, QStringLiteral("EHLO %1").arg(localName), QStringLiteral("EHLO after STARTTLS"), QStringList{QStringLiteral("250")});
        writeSmtpLine(socket, QStringLiteral("AUTH LOGIN"), QStringLiteral("AUTH LOGIN"), QStringList{QStringLiteral("334")});
        writeSmtpLine(socket, QString::fromUtf8(request.senderEmail.trimmed().toUtf8().toBase64()), QStringLiteral("SMTP username"), QStringList{QStringLiteral("334")});
        writeSmtpLine(socket, QString::fromUtf8(request.senderPassword.toUtf8().toBase64()), QStringLiteral("SMTP password"), QStringList{QStringLiteral("235")});
        writeSmtpLine(socket, QStringLiteral("MAIL FROM:<%1>").arg(request.senderEmail.trimmed()), QStringLiteral("MAIL FROM"), QStringList{QStringLiteral("250")});
        for (const QString& recipient : toRecipients + ccRecipients) {
            writeSmtpLine(socket, QStringLiteral("RCPT TO:<%1>").arg(recipient), QStringLiteral("RCPT TO"), QStringList{QStringLiteral("250"), QStringLiteral("251")});
        }
        writeSmtpLine(socket, QStringLiteral("DATA"), QStringLiteral("DATA"), QStringList{QStringLiteral("354")});
        socket.write(message.toUtf8());
        socket.write("\r\n.\r\n");
        if (!socket.waitForBytesWritten(15000)) {
            throw domain::DomainError("SMTP write timeout while sending message body");
        }
        expectSmtpCode(socket, QStringLiteral("message body"), QStringList{QStringLiteral("250")});
        writeSmtpLine(socket, QStringLiteral("QUIT"), QStringLiteral("QUIT"), QStringList{QStringLiteral("221")});
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

    void applyDayValues(QSqlDatabase database, const QString& tableName, int itemId, const QString& company, const QString& financialYear, int month, const QString& prefix, QJsonObject& item) const {
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
            updateQuery.prepare(QStringLiteral("UPDATE inventory_items SET name=?, cost=?, min_stock=?, updated_at=GETDATE() WHERE id=?"));
            updateQuery.addBindValue(name);
            updateQuery.addBindValue(cost);
            updateQuery.addBindValue(minStock);
            updateQuery.addBindValue(itemId);
            executePrepared(updateQuery, QStringLiteral("Update inventory item"));
            return itemId;
        }

        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral("INSERT INTO inventory_items (client_row_id, company, name, cost, min_stock) OUTPUT INSERTED.id VALUES (?, ?, ?, ?, ?)"));
        insertQuery.addBindValue(rowId);
        insertQuery.addBindValue(company);
        insertQuery.addBindValue(name);
        insertQuery.addBindValue(cost);
        insertQuery.addBindValue(minStock);
        executePrepared(insertQuery, QStringLiteral("Create inventory item"));
        if (!insertQuery.next()) {
            throw domain::DomainError(QStringLiteral("Inventory item insert did not return an id: %1").arg(name).toStdString());
        }
        return insertQuery.value(0).toInt();
    }

    void replaceDayValues(QSqlDatabase database, const QString& tableName, int itemId, const QString& company, const QString& financialYear, int month, const QString& prefix, const QJsonObject& row) const {
        QSqlQuery deleteQuery(database);
        deleteQuery.prepare(QStringLiteral("DELETE FROM %1 WHERE item_id=? AND company=? AND financial_year=? AND month=?").arg(tableName));
        deleteQuery.addBindValue(itemId);
        deleteQuery.addBindValue(company);
        deleteQuery.addBindValue(financialYear);
        deleteQuery.addBindValue(month);
        executePrepared(deleteQuery, QStringLiteral("Clear inventory day values"));

        for (int day = 1; day <= 31; day += 1) {
            const double quantity = row.value(dayKey(prefix, day)).toDouble();
            if (std::abs(quantity) < 0.005) {
                continue;
            }
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

class NativeBackupService final : public domain::BackupService, private SqlServiceBase {
public:
    explicit NativeBackupService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    domain::BackupResult backup(const QString& destinationPath) override {
        domain::DatabaseConfig config = readConfig();
        const QString requestedPath = destinationPath.trimmed().isEmpty() ? config.backupDir : destinationPath.trimmed();
        if (requestedPath.isEmpty()) {
            throw domain::DomainError("Backup destination path is required");
        }

        QDir directory(requestedPath);
        if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
            throw domain::DomainError(QStringLiteral("Could not create backup directory: %1").arg(requestedPath).toStdString());
        }

        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString backupPath = directory.filePath(QStringLiteral("%1_%2.bak").arg(config.database, timestamp));
        SqlDatabase database(config);
        if (!database.open()) {
            throw domain::DomainError("Could not open database while backing up");
        }

        QSqlQuery query(database.handle());
        const QString sql = QStringLiteral("BACKUP DATABASE [%1] TO DISK = '%2' WITH INIT, COPY_ONLY")
            .arg(escapeSqlIdentifier(config.database), escapeSqlLiteral(backupPath));
        if (!query.exec(sql)) {
            throwSqlError(QStringLiteral("Backup database"), query);
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

        domain::DatabaseConfig config = readConfig();
        const QString databaseName = config.database;
        config.database = QStringLiteral("master");

        SqlDatabase database(config);
        if (!database.open()) {
            throw domain::DomainError("Could not open master database while restoring backup");
        }

        QSqlQuery query(database.handle());
        const QString escapedDatabase = escapeSqlIdentifier(databaseName);
        const QString escapedPath = escapeSqlLiteral(backupFile.absoluteFilePath());
        const QString sql = QStringLiteral(
            "ALTER DATABASE [%1] SET SINGLE_USER WITH ROLLBACK IMMEDIATE; "
            "RESTORE DATABASE [%1] FROM DISK = '%2' WITH REPLACE; "
            "ALTER DATABASE [%1] SET MULTI_USER"
        ).arg(escapedDatabase, escapedPath);
        if (!query.exec(sql)) {
            throwSqlError(QStringLiteral("Restore database"), query);
        }
    }
};

class NativeAuditService final : public domain::AuditService, private SqlServiceBase {
public:
    explicit NativeAuditService(const std::shared_ptr<domain::ConfigService>& configService)
        : SqlServiceBase(configService) {}

    QJsonArray listAuditLogs(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while listing audit logs");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));

        QSqlQuery query(database.handle());
        if (!query.exec(QStringLiteral("SELECT TOP 500 timestamp, username, action, details FROM audit_logs ORDER BY timestamp DESC"))) {
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
        SqlDatabase database(readConfig());
        if (!database.open()) {
            throw domain::DomainError("Could not open database while writing audit log");
        }
        SchemaInitializer(database.handle()).initialize(QStringLiteral("default"));
        writeAudit(database.handle(), username, action, details);
    }
};

class NativeUpdateService final : public domain::UpdateService {
public:
    void checkForUpdates() override {
        return;
    }

    void restartAndInstall() override {
        return;
    }
};

} // namespace

domain::ServiceRegistry createSqlBusinessServices(const std::shared_ptr<domain::ConfigService>& configService) {
    domain::ServiceRegistry registry;
    registry.config = configService;
    registry.auth = std::make_shared<NativeAuthService>(configService);
    registry.users = std::make_shared<NativeUserService>(configService);
    registry.companies = std::make_shared<NativeCompanyService>(configService);
    registry.financialYears = std::make_shared<NativeFinancialYearService>(configService);
    registry.transactions = std::make_shared<NativeTransactionService>(configService);
    registry.parties = std::make_shared<NativePartyService>(configService);
    registry.reports = std::make_shared<NativeReportService>(configService);
    registry.inventory = std::make_shared<NativeInventoryService>(configService);
    registry.backups = std::make_shared<NativeBackupService>(configService);
    registry.audit = std::make_shared<NativeAuditService>(configService);
    registry.updates = std::make_shared<NativeUpdateService>();
    return registry;
}

} // namespace mfinlogs::data
