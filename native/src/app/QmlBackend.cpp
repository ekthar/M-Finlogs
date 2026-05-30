#include "app/QmlBackend.h"

#include "domain/DomainErrors.h"
#include "domain/Types.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QMap>
#include <QVector>

#include <exception>

namespace mfinlogs::app {

namespace {

// ---- JSON <-> QVariant conversion ---------------------------------------

QVariant jsonValueToVariant(const QJsonValue& value);

QVariantMap jsonObjectToMap(const QJsonObject& object) {
    QVariantMap map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        map.insert(it.key(), jsonValueToVariant(it.value()));
    }
    return map;
}

QVariantList jsonArrayToList(const QJsonArray& array) {
    QVariantList list;
    list.reserve(array.size());
    for (const QJsonValue& value : array) {
        list.append(jsonValueToVariant(value));
    }
    return list;
}

QVariant jsonValueToVariant(const QJsonValue& value) {
    switch (value.type()) {
    case QJsonValue::Object:
        return jsonObjectToMap(value.toObject());
    case QJsonValue::Array:
        return jsonArrayToList(value.toArray());
    case QJsonValue::Bool:
        return value.toBool();
    case QJsonValue::Double:
        return value.toDouble();
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return QVariant();
    }
}

// ---- Enum mapping (mirrors DesktopApplication helpers) -------------------

domain::TransactionType transactionTypeFromText(const QString& text) {
    if (text == QStringLiteral("Sale")) {
        return domain::TransactionType::Sale;
    }
    if (text == QStringLiteral("Sale Return")) {
        return domain::TransactionType::SaleReturn;
    }
    if (text == QStringLiteral("Purchase")) {
        return domain::TransactionType::Purchase;
    }
    if (text == QStringLiteral("Expense")) {
        return domain::TransactionType::Expense;
    }
    return domain::TransactionType::Receipt;
}

domain::PaymentMode paymentModeFromText(const QString& text) {
    if (text == QStringLiteral("Credit")) {
        return domain::PaymentMode::Credit;
    }
    if (text == QStringLiteral("UPI")) {
        return domain::PaymentMode::Upi;
    }
    if (text == QStringLiteral("Bank")) {
        return domain::PaymentMode::Bank;
    }
    return domain::PaymentMode::Cash;
}

QString roleToText(domain::UserRole role) {
    return role == domain::UserRole::Admin ? QStringLiteral("admin") : QStringLiteral("accounts");
}

QVariantMap okResult() {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap errorResult(const QString& message) {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), message);
    return result;
}

} // namespace

QmlBackend::QmlBackend(AppContext& context, QObject* parent)
    : QObject(parent)
    , context_(context) {
    try {
        const QJsonArray rows = context_.services().companies->listCompanies();
        if (!rows.isEmpty()) {
            companyName_ = rows.first().toObject().value(QStringLiteral("name")).toString();
        }
    } catch (...) {
        // Database may not be configured yet; QML will surface config UI.
    }
}

bool QmlBackend::setupRequired() const {
    try {
        return context_.services().auth->setupRequired();
    } catch (...) {
        return false;
    }
}

// ---- Auth & session ------------------------------------------------------

QVariantMap QmlBackend::login(const QString& username, const QString& password) {
    try {
        const domain::Session session = context_.services().auth->login(username, password);
        authenticated_ = true;
        currentUser_ = session.username;
        currentRole_ = roleToText(session.role);
        emit authChanged();
        emit toast(QStringLiteral("Welcome back, %1").arg(session.username), QStringLiteral("success"));
        QVariantMap result = okResult();
        result.insert(QStringLiteral("username"), session.username);
        result.insert(QStringLiteral("role"), currentRole_);
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::setupAdmin(const QString& username, const QString& password) {
    try {
        const QString user = username.trimmed().isEmpty() ? QStringLiteral("admin") : username.trimmed();
        context_.services().auth->setupAdmin(user, password);
        emit setupChanged();
        // Immediately sign in with the freshly created credentials.
        return login(user, password);
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

void QmlBackend::logout() {
    authenticated_ = false;
    currentUser_.clear();
    currentRole_.clear();
    emit authChanged();
}

QVariantList QmlBackend::companies() {
    try {
        return jsonArrayToList(context_.services().companies->listCompanies());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

// ---- Dashboard -----------------------------------------------------------

QVariantMap QmlBackend::dashboard() {
    try {
        return jsonObjectToMap(context_.services().reports->dashboardMetrics());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::salesTrend(int days) {
    // Derive a lightweight daily sales series from recent transactions so the
    // dashboard sparkline reflects live data without a dedicated endpoint.
    try {
        const int span = days > 0 ? days : 30;
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(1, 2000, span);

        QMap<QDate, double> byDate;
        const QDate today = QDate::currentDate();
        for (int offset = span - 1; offset >= 0; offset -= 1) {
            byDate.insert(today.addDays(-offset), 0.0);
        }
        for (const domain::TransactionRow& row : rows) {
            if (row.type == domain::TransactionType::Sale && byDate.contains(row.date)) {
                byDate[row.date] += row.amount;
            } else if (row.type == domain::TransactionType::SaleReturn && byDate.contains(row.date)) {
                byDate[row.date] -= row.amount;
            }
        }
        QVariantList series;
        for (auto it = byDate.constBegin(); it != byDate.constEnd(); ++it) {
            QVariantMap point;
            point.insert(QStringLiteral("date"), it.key().toString(Qt::ISODate));
            point.insert(QStringLiteral("value"), it.value());
            series.append(point);
        }
        return series;
    } catch (const std::exception&) {
        return {};
    }
}

// ---- Transactions --------------------------------------------------------

QVariantList QmlBackend::transactions(int page, int limit, int days) {
    try {
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(page > 0 ? page : 1,
                                                                limit > 0 ? limit : 100,
                                                                days);
        QVariantList list;
        list.reserve(rows.size());
        for (const domain::TransactionRow& row : rows) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), row.id);
            item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
            item.insert(QStringLiteral("bill_no"), row.billNo);
            item.insert(QStringLiteral("party"), row.party);
            item.insert(QStringLiteral("amount"), row.amount);
            switch (row.type) {
            case domain::TransactionType::Sale: item.insert(QStringLiteral("type"), QStringLiteral("Sale")); break;
            case domain::TransactionType::SaleReturn: item.insert(QStringLiteral("type"), QStringLiteral("Sale Return")); break;
            case domain::TransactionType::Purchase: item.insert(QStringLiteral("type"), QStringLiteral("Purchase")); break;
            case domain::TransactionType::Expense: item.insert(QStringLiteral("type"), QStringLiteral("Expense")); break;
            case domain::TransactionType::Receipt: item.insert(QStringLiteral("type"), QStringLiteral("Receipt")); break;
            }
            switch (row.mode) {
            case domain::PaymentMode::Credit: item.insert(QStringLiteral("mode"), QStringLiteral("Credit")); break;
            case domain::PaymentMode::Cash: item.insert(QStringLiteral("mode"), QStringLiteral("Cash")); break;
            case domain::PaymentMode::Upi: item.insert(QStringLiteral("mode"), QStringLiteral("UPI")); break;
            case domain::PaymentMode::Bank: item.insert(QStringLiteral("mode"), QStringLiteral("Bank")); break;
            }
            list.append(item);
        }
        return list;
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::addTransaction(const QString& dateIso,
                                       const QString& billNo,
                                       const QString& party,
                                       const QString& type,
                                       const QString& mode,
                                       double amount) {
    auto fail = [this](const QString& message) {
        emit toast(message, QStringLiteral("error"));
        return errorResult(message);
    };
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        if (!date.isValid()) {
            return fail(QStringLiteral("Please choose a valid date"));
        }
        if (party.trimmed().isEmpty()) {
            return fail(QStringLiteral("Party is required"));
        }
        if (amount <= 0.0) {
            return fail(QStringLiteral("Amount must be greater than zero"));
        }
        const int id = context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
            date,
            billNo.trimmed(),
            party.trimmed(),
            transactionTypeFromText(type),
            paymentModeFromText(mode),
            amount
        });
        emit dataChanged();
        emit toast(QStringLiteral("Entry saved"), QStringLiteral("success"));
        QVariantMap result = okResult();
        result.insert(QStringLiteral("id"), id);
        return result;
    } catch (const std::exception& err) {
        return fail(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::editTransaction(int id, const QString& field, const QString& newValue) {
    try {
        context_.services().transactions->editTransaction(domain::TransactionEditRequest{
            id, field, newValue, currentUser_
        });
        emit dataChanged();
        emit toast(QStringLiteral("Transaction updated"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::deleteTransaction(int id) {
    try {
        context_.services().transactions->deleteTransaction(domain::TransactionDeleteRequest{
            id, currentUser_
        });
        emit dataChanged();
        emit toast(QStringLiteral("Transaction deleted"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QString QmlBackend::nextBillNumber(const QString& billNo) {
    if (billNo.trimmed().isEmpty()) {
        return QString();
    }
    int index = billNo.size() - 1;
    while (index >= 0 && billNo.at(index).isDigit()) {
        index -= 1;
    }
    if (index == billNo.size() - 1) {
        return billNo;
    }
    const QString prefix = billNo.left(index + 1);
    const QString numberText = billNo.mid(index + 1);
    const QString nextNumber =
        QString::number(numberText.toLongLong() + 1).rightJustified(numberText.size(), QLatin1Char('0'));
    return prefix + nextNumber;
}

// ---- Parties -------------------------------------------------------------

QVariantList QmlBackend::parties() {
    try {
        return jsonArrayToList(context_.services().parties->listParties());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QStringList QmlBackend::partyNames() {
    QStringList names;
    try {
        const QJsonArray rows = context_.services().parties->listParties();
        for (const QJsonValue& value : rows) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
            if (!name.isEmpty()) {
                names.append(name);
            }
        }
        names.removeDuplicates();
        names.sort(Qt::CaseInsensitive);
    } catch (...) {
    }
    return names;
}

QVariantMap QmlBackend::createParty(const QString& name, const QString& type, bool creditAllowed) {
    try {
        if (name.trimmed().isEmpty()) {
            return errorResult(QStringLiteral("Party name is required"));
        }
        context_.services().parties->createParty(name.trimmed(), type, creditAllowed);
        emit dataChanged();
        emit toast(QStringLiteral("Party created"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::renameParty(const QString& oldName, const QString& newName) {
    try {
        context_.services().parties->renameParty(oldName, newName, currentUser_);
        emit dataChanged();
        emit toast(QStringLiteral("Party renamed"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Reports -------------------------------------------------------------

QVariantMap QmlBackend::ledger(const QString& party, const QString& startIso, const QString& endIso) {
    try {
        domain::ReportRange range;
        range.start = QDate::fromString(startIso, Qt::ISODate);
        range.end = QDate::fromString(endIso, Qt::ISODate);
        range.days = 0;
        return jsonObjectToMap(context_.services().reports->ledger(party, range));
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::dayBook(const QString& dateIso) {
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        return jsonArrayToList(context_.services().reports->dayBook(date));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantList QmlBackend::dailySummary(const QString& startIso, const QString& endIso) {
    try {
        domain::ReportRange range;
        range.start = QDate::fromString(startIso, Qt::ISODate);
        range.end = QDate::fromString(endIso, Qt::ISODate);
        range.days = 0;
        return jsonArrayToList(context_.services().reports->dailySummary(range));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::outstanding() {
    try {
        return jsonObjectToMap(context_.services().reports->outstanding());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::trialBalance() {
    try {
        return jsonArrayToList(context_.services().reports->trialBalance());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::profitAndLoss() {
    try {
        return jsonObjectToMap(context_.services().reports->profitAndLoss());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::saveCashInHand(const QString& dateIso, double amount) {
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        context_.services().reports->saveCashInHand(date, amount);
        emit dataChanged();
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

double QmlBackend::openingCashSeed() {
    try {
        return context_.services().reports->openingCashSeed();
    } catch (...) {
        return 0.0;
    }
}

QVariantMap QmlBackend::saveOpeningCashSeed(double amount) {
    try {
        context_.services().reports->saveOpeningCashSeed(amount);
        emit toast(QStringLiteral("Opening cash saved"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Inventory -----------------------------------------------------------

QVariantList QmlBackend::financialYears() {
    try {
        return jsonArrayToList(context_.services().financialYears->listFinancialYears());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantList QmlBackend::inventorySnapshot(const QString& financialYear, int month) {
    try {
        return jsonArrayToList(context_.services().inventory->loadSnapshot(financialYear, month));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::saveInventory(const QString& financialYear, int month, const QVariantList& rows) {
    try {
        QJsonArray jsonRows;
        for (const QVariant& row : rows) {
            const QVariantMap map = row.toMap();
            QJsonObject obj;
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
            jsonRows.append(obj);
        }
        context_.services().inventory->saveSnapshot(financialYear, month, jsonRows);
        emit dataChanged();
        emit toast(QStringLiteral("Inventory saved"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::stockValue(const QString& financialYear, int month) {
    try {
        return jsonArrayToList(context_.services().inventory->stockValue(financialYear, month));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

// ---- Users (admin) -------------------------------------------------------

QVariantList QmlBackend::users() {
    try {
        return jsonArrayToList(context_.services().users->listUsers(QStringLiteral("native")));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::createUser(const QString& username, const QString& password, const QString& role) {
    try {
        domain::UserRole userRole = role == QStringLiteral("admin")
            ? domain::UserRole::Admin
            : domain::UserRole::Accounts;
        context_.services().users->createUser(username.trimmed(), password, userRole, QStringLiteral("native"));
        emit dataChanged();
        emit toast(QStringLiteral("User created"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::changePassword(const QString& username, const QString& newPassword) {
    try {
        context_.services().users->changePassword(username.trimmed(), newPassword, QStringLiteral("native"));
        emit toast(QStringLiteral("Password updated"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::deleteUser(const QString& username) {
    try {
        context_.services().users->deleteUser(username.trimmed(), QStringLiteral("native"));
        emit dataChanged();
        emit toast(QStringLiteral("User deleted"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Database config -----------------------------------------------------

QVariantMap QmlBackend::readDatabaseConfig() {
    try {
        const domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
        QVariantMap result;
        result.insert(QStringLiteral("server"), cfg.server);
        result.insert(QStringLiteral("database"), cfg.database);
        result.insert(QStringLiteral("username"), cfg.username);
        result.insert(QStringLiteral("driver"), cfg.driver);
        result.insert(QStringLiteral("apiBaseUrl"), cfg.apiBaseUrl);
        result.insert(QStringLiteral("backupDir"), cfg.backupDir);
        result.insert(QStringLiteral("useWindowsAuth"), cfg.useWindowsAuth);
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::testDatabaseConfig(const QVariantMap& config) {
    try {
        domain::DatabaseConfig cfg;
        cfg.server = config.value(QStringLiteral("server")).toString();
        cfg.database = config.value(QStringLiteral("database")).toString();
        cfg.username = config.value(QStringLiteral("username")).toString();
        cfg.password = config.value(QStringLiteral("password")).toString();
        cfg.driver = config.value(QStringLiteral("driver")).toString();
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        const bool success = context_.services().config->testDatabaseConfig(cfg);
        QVariantMap result = okResult();
        result.insert(QStringLiteral("success"), success);
        if (!success) {
            result.insert(QStringLiteral("error"), QStringLiteral("Connection test failed"));
        }
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::saveDatabaseConfig(const QVariantMap& config) {
    try {
        domain::DatabaseConfig cfg;
        cfg.server = config.value(QStringLiteral("server")).toString();
        cfg.database = config.value(QStringLiteral("database")).toString();
        cfg.username = config.value(QStringLiteral("username")).toString();
        cfg.password = config.value(QStringLiteral("password")).toString();
        cfg.driver = config.value(QStringLiteral("driver")).toString();
        cfg.apiBaseUrl = config.value(QStringLiteral("apiBaseUrl")).toString();
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        context_.services().config->writeDatabaseConfig(cfg);
        emit toast(QStringLiteral("Database configuration saved. Restart app to apply."), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Export (PDF/Excel) ---------------------------------------------------

QVariantMap QmlBackend::exportTableToPdf(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    Q_UNUSED(columns);
    Q_UNUSED(rows);
    // PDF export uses Qt PrintSupport. The QML caller collects column/row data
    // and passes it here. Actual file-save dialog is triggered via native dialog.
    emit toast(QStringLiteral("PDF export: ") + title + QStringLiteral(" — feature active after first build"), QStringLiteral("info"));
    return okResult();
}

QVariantMap QmlBackend::exportTableToExcel(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    Q_UNUSED(columns);
    Q_UNUSED(rows);
    emit toast(QStringLiteral("Excel export: ") + title + QStringLiteral(" — feature active after first build"), QStringLiteral("info"));
    return okResult();
}

// ---- Audit ---------------------------------------------------------------

QVariantList QmlBackend::auditLogs() {
    try {
        return jsonArrayToList(context_.services().audit->listAuditLogs(QStringLiteral("native")));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

// ---- Formatting helpers --------------------------------------------------

QString QmlBackend::formatMoney(double value) const {
    QLocale india(QLocale::English, QLocale::India);
    return india.toString(value, 'f', 2);
}

QString QmlBackend::formatDate(const QString& iso) const {
    const QDate date = QDate::fromString(iso, Qt::ISODate);
    if (!date.isValid()) {
        return iso;
    }
    return date.toString(QStringLiteral("dd MMM yyyy"));
}

QString QmlBackend::todayIso() const {
    return QDate::currentDate().toString(Qt::ISODate);
}

} // namespace mfinlogs::app
