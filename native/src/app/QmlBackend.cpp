#include "app/QmlBackend.h"

#include "domain/DomainErrors.h"
#include "domain/Types.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
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
        // If no party specified, default to "customer" for quick cash sales
        const QString resolvedParty = party.trimmed().isEmpty()
            ? QStringLiteral("customer")
            : party.trimmed();
        // Credit mode is not allowed for the generic "customer" party
        if (resolvedParty == QStringLiteral("customer") && mode == QStringLiteral("Credit")) {
            return fail(QStringLiteral("Credit mode is not allowed for generic 'customer'. Select a Credit Customer or choose Cash/UPI/Bank."));
        }
        if (amount <= 0.0) {
            return fail(QStringLiteral("Amount must be greater than zero"));
        }
        const int id = context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
            date,
            billNo.trimmed(),
            resolvedParty,
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

QVariantMap QmlBackend::inventoryPdfPreview(const QString& financialYear, int month, bool onlyReorder) {
    try {
        // Load the snapshot and convert to InventoryProductRow vector
        const QJsonArray snapshot = context_.services().inventory->loadSnapshot(financialYear, month);
        QVector<domain::InventoryProductRow> rows;
        for (const QJsonValue& value : snapshot) {
            const QJsonObject obj = value.toObject();
            domain::InventoryProductRow row;
            row.name = obj.value(QStringLiteral("name")).toString();
            row.cost = obj.value(QStringLiteral("cost")).toDouble();
            row.minStock = obj.value(QStringLiteral("min_stock")).toDouble();
            for (int day = 1; day <= 31; ++day) {
                const QString qk = QStringLiteral("qty_%1").arg(day, 2, 10, QLatin1Char('0'));
                const QString pk = QStringLiteral("purchase_%1").arg(day, 2, 10, QLatin1Char('0'));
                row.quantities.append(obj.value(qk).toDouble());
                row.purchaseQuantities.append(obj.value(pk).toDouble());
            }
            rows.append(row);
        }
        if (rows.isEmpty()) {
            return errorResult(QStringLiteral("No inventory data to export"));
        }

        const QString monthName = QDate(2000, month, 1).toString(QStringLiteral("MMMM"));
        domain::InventoryPdfMailRequest request{
            QString(), QString(), QString(), QString(),
            QStringLiteral("smtp.gmail.com"), 587,
            QStringLiteral("Inventory Report"), QString(),
            financialYear, monthName, QStringLiteral("last7"),
            onlyReorder, true, rows
        };

        const QByteArray pdf = context_.services().inventory->buildPdfPreview(request);

        // Write to a temp file and open it
        const QString tempPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("MFinlogs_Inventory_%1.pdf")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return errorResult(QStringLiteral("Could not write preview PDF"));
        }
        tempFile.write(pdf);
        tempFile.close();

        QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
        emit toast(QStringLiteral("Inventory PDF report opened"), QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), tempPath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
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
        if (cfg.driver.trimmed().isEmpty()) {
            cfg.driver = QStringLiteral("{ODBC Driver 17 for SQL Server}");
        }
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        // testDatabaseConfig throws on failure with a descriptive message;
        // it returns true only on success.
        context_.services().config->testDatabaseConfig(cfg);
        QVariantMap result = okResult();
        result.insert(QStringLiteral("success"), true);
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
        if (cfg.driver.trimmed().isEmpty()) {
            cfg.driver = QStringLiteral("{ODBC Driver 17 for SQL Server}");
        }
        cfg.apiBaseUrl = config.value(QStringLiteral("apiBaseUrl")).toString();
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        // Test first, then save (same as the reference commit's behavior)
        context_.services().config->testDatabaseConfig(cfg);
        context_.services().config->writeDatabaseConfig(cfg);
        emit toast(QStringLiteral("Database configuration saved. Restart app to apply."), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Backup & restore ----------------------------------------------------

QVariantMap QmlBackend::backupDatabase(const QString& targetDir) {
    try {
        const domain::BackupResult result = context_.services().backups->backup(targetDir.trimmed());
        emit toast(result.status + QStringLiteral(": ") + result.path, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), result.path);
        res.insert(QStringLiteral("status"), result.status);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::autoBackup() {
    try {
        const domain::BackupResult result = context_.services().backups->autoBackup();
        emit toast(result.status + QStringLiteral(": ") + result.path, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), result.path);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::restoreDatabase(const QString& backupPath) {
    try {
        if (backupPath.trimmed().isEmpty()) {
            return errorResult(QStringLiteral("No backup file selected"));
        }
        context_.services().backups->restore(backupPath.trimmed());
        emit toast(QStringLiteral("Backup restored. Restart app to reload."), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Server controls -----------------------------------------------------

QVariantMap QmlBackend::startNativeServer() {
    const bool launched = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        {QStringLiteral("--server-start")}
    );
    if (launched) {
        emit toast(QStringLiteral("Native server start requested"), QStringLiteral("success"));
        return okResult();
    }
    return errorResult(QStringLiteral("Could not start native server process"));
}

QVariantMap QmlBackend::stopNativeServer() {
    const bool launched = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        {QStringLiteral("--server-stop")}
    );
    if (launched) {
        emit toast(QStringLiteral("Native server stop requested"), QStringLiteral("success"));
        return okResult();
    }
    return errorResult(QStringLiteral("Could not stop native server process"));
}

// ---- Export (PDF/Excel) ---------------------------------------------------

QVariantMap QmlBackend::exportTableToPdf(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    try {
        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/") + title.simplified().replace(QLatin1Char(' '), QLatin1Char('_')) + QStringLiteral(".pdf");
        const QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Export PDF"), defaultPath, QStringLiteral("PDF (*.pdf)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult(); // User cancelled
        }

        QPdfWriter writer(filePath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Landscape);
        writer.setResolution(96);
        writer.setTitle(title);

        QPainter painter(&writer);
        if (!painter.isActive()) {
            return errorResult(QStringLiteral("Could not start PDF painter"));
        }

        const int pageW = writer.width();
        const int margin = 60;
        int y = margin;

        // Title
        QFont titleFont(QStringLiteral("Inter Tight"), 18, QFont::Bold);
        painter.setFont(titleFont);
        painter.setPen(QColor(0x1e, 0x24, 0x35));
        painter.drawText(margin, y + 22, title);
        y += 50;

        // Subtitle (date)
        QFont subFont(QStringLiteral("Inter Tight"), 9);
        painter.setFont(subFont);
        painter.setPen(QColor(0x64, 0x74, 0x8b));
        painter.drawText(margin, y + 12, QStringLiteral("Generated: ") + QDate::currentDate().toString(QStringLiteral("dd MMM yyyy")));
        y += 30;

        // Table
        if (columns.isEmpty()) {
            painter.end();
            emit toast(QStringLiteral("PDF exported: ") + filePath, QStringLiteral("success"));
            QVariantMap res = okResult();
            res.insert(QStringLiteral("path"), filePath);
            return res;
        }

        const int colCount = columns.size();
        const int colW = (pageW - margin * 2) / colCount;
        const int rowH = 28;

        // Header row
        QFont headerFont(QStringLiteral("Inter Tight"), 8, QFont::Bold);
        painter.setFont(headerFont);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x1b, 0x24, 0x40));
        painter.drawRect(margin, y, pageW - margin * 2, rowH);
        painter.setPen(QColor(0xee, 0xf2, 0xfb));
        for (int c = 0; c < colCount; ++c) {
            const QString col = columns[c].toString();
            painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH), Qt::AlignVCenter | Qt::AlignLeft, col);
        }
        y += rowH;

        // Data rows
        QFont dataFont(QStringLiteral("Inter Tight"), 8);
        painter.setFont(dataFont);
        for (int r = 0; r < rows.size(); ++r) {
            if (y + rowH > writer.height() - margin) {
                writer.newPage();
                y = margin;
            }
            const QVariantList row = rows[r].toList();
            painter.setPen(Qt::NoPen);
            painter.setBrush(r % 2 == 0 ? QColor(0xf8, 0xfa, 0xfc) : QColor(0xff, 0xff, 0xff));
            painter.drawRect(margin, y, pageW - margin * 2, rowH);
            painter.setPen(QColor(0x1e, 0x24, 0x35));
            for (int c = 0; c < qMin(colCount, row.size()); ++c) {
                painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH), Qt::AlignVCenter | Qt::AlignLeft, row[c].toString());
            }
            // Bottom border
            painter.setPen(QPen(QColor(0xe5, 0xe7, 0xeb), 1));
            painter.drawLine(margin, y + rowH, pageW - margin, y + rowH);
            y += rowH;
        }

        painter.end();
        emit toast(QStringLiteral("PDF exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::exportTableToExcel(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    // CSV export (universally openable in Excel) — no external lib needed
    try {
        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/") + title.simplified().replace(QLatin1Char(' '), QLatin1Char('_')) + QStringLiteral(".csv");
        const QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Export CSV/Excel"), defaultPath, QStringLiteral("CSV (*.csv)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return errorResult(QStringLiteral("Could not write file: ") + filePath);
        }
        QTextStream stream(&file);

        // Header
        QStringList headers;
        for (const QVariant& col : columns) {
            headers.append(QStringLiteral("\"%1\"").arg(col.toString().replace(QLatin1Char('"'), QStringLiteral("\"\""))));
        }
        stream << headers.join(QLatin1Char(',')) << QStringLiteral("\n");

        // Data
        for (const QVariant& rowVar : rows) {
            const QVariantList row = rowVar.toList();
            QStringList cells;
            for (const QVariant& cell : row) {
                cells.append(QStringLiteral("\"%1\"").arg(cell.toString().replace(QLatin1Char('"'), QStringLiteral("\"\""))));
            }
            stream << cells.join(QLatin1Char(',')) << QStringLiteral("\n");
        }

        file.close();
        emit toast(QStringLiteral("CSV exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- 7-day Recent Transactions PDF Report --------------------------------

QVariantMap QmlBackend::exportRecentPdf(int days) {
    try {
        const int span = days > 0 ? days : 7;
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(1, 5000, span);

        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/M-Finlogs_Last_%1_Days.pdf").arg(span);
        const QString filePath = QFileDialog::getSaveFileName(nullptr,
            QStringLiteral("Export %1-Day Report").arg(span), defaultPath, QStringLiteral("PDF (*.pdf)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QPdfWriter writer(filePath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Landscape);
        writer.setResolution(96);
        writer.setTitle(QStringLiteral("M-Finlogs — Last %1 Days").arg(span));

        QPainter painter(&writer);
        if (!painter.isActive()) {
            return errorResult(QStringLiteral("Could not start PDF writer"));
        }

        const int pageW = writer.width();
        const int pageH = writer.height();
        const int margin = 32;
        const int tableW = pageW - margin * 2;
        int y = margin;

        // --- Title block ---
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 16, QFont::Bold));
        painter.setPen(QColor(0x1e, 0x24, 0x35));
        painter.drawText(margin, y + 20, QStringLiteral("M-Finlogs — Transaction Report"));
        y += 30;
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 9));
        painter.setPen(QColor(0x64, 0x74, 0x8b));
        const QDate today = QDate::currentDate();
        painter.drawText(margin, y + 12,
            QStringLiteral("Period: %1 to %2 | Generated: %3")
                .arg(today.addDays(-span + 1).toString(QStringLiteral("dd MMM yyyy")),
                     today.toString(QStringLiteral("dd MMM yyyy")),
                     today.toString(QStringLiteral("dd MMM yyyy"))));
        y += 28;

        // --- Column layout ---
        const QStringList headers = {
            QStringLiteral("Date"), QStringLiteral("Bill No"), QStringLiteral("Party"),
            QStringLiteral("Type"), QStringLiteral("Mode"), QStringLiteral("Amount")
        };
        const QVector<double> colWeights = {1.1, 1.0, 2.2, 1.0, 0.9, 1.2};
        double totalWeight = 0;
        for (double w : colWeights) totalWeight += w;
        QVector<int> colWidths;
        for (double w : colWeights) colWidths.append(static_cast<int>(tableW * w / totalWeight));

        const int rowH = 22;
        const int headerH = 26;

        auto drawTableHeader = [&](int& yPos) {
            // Dark header bar
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0x1b, 0x24, 0x40));
            painter.drawRect(margin, yPos, tableW, headerH);
            painter.setPen(QColor(0xee, 0xf2, 0xfb));
            painter.setFont(QFont(QStringLiteral("Inter Tight"), 8, QFont::Bold));
            int x = margin;
            for (int c = 0; c < headers.size(); ++c) {
                const Qt::Alignment align = (c == 5) ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
                painter.drawText(QRect(x + 6, yPos, colWidths[c] - 12, headerH), align, headers[c]);
                x += colWidths[c];
            }
            yPos += headerH;
        };

        drawTableHeader(y);

        // --- Data rows ---
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 8));
        double grandTotal = 0.0;
        for (int r = 0; r < rows.size(); ++r) {
            if (y + rowH > pageH - margin) {
                writer.newPage();
                y = margin;
                drawTableHeader(y);
                painter.setFont(QFont(QStringLiteral("Inter Tight"), 8));
            }

            // Alternating bands
            painter.setPen(Qt::NoPen);
            painter.setBrush(r % 2 == 0 ? QColor(0xf8, 0xfa, 0xfc) : QColor(0xff, 0xff, 0xff));
            painter.drawRect(margin, y, tableW, rowH);

            const domain::TransactionRow& row = rows[r];
            QString typeText;
            switch (row.type) {
            case domain::TransactionType::Sale: typeText = QStringLiteral("Sale"); break;
            case domain::TransactionType::SaleReturn: typeText = QStringLiteral("Sale Return"); break;
            case domain::TransactionType::Purchase: typeText = QStringLiteral("Purchase"); break;
            case domain::TransactionType::Expense: typeText = QStringLiteral("Expense"); break;
            case domain::TransactionType::Receipt: typeText = QStringLiteral("Receipt"); break;
            }
            QString modeText;
            switch (row.mode) {
            case domain::PaymentMode::Credit: modeText = QStringLiteral("Credit"); break;
            case domain::PaymentMode::Cash: modeText = QStringLiteral("Cash"); break;
            case domain::PaymentMode::Upi: modeText = QStringLiteral("UPI"); break;
            case domain::PaymentMode::Bank: modeText = QStringLiteral("Bank"); break;
            }
            const QStringList cells = {
                row.date.toString(QStringLiteral("dd-MM-yyyy")),
                row.billNo,
                row.party,
                typeText,
                modeText,
                formatMoney(row.amount)
            };

            painter.setPen(QColor(0x1e, 0x24, 0x35));
            int x = margin;
            for (int c = 0; c < cells.size(); ++c) {
                const Qt::Alignment align = (c == 5) ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
                painter.drawText(QRect(x + 6, y, colWidths[c] - 12, rowH), align, cells[c]);
                x += colWidths[c];
            }
            // Row border
            painter.setPen(QPen(QColor(0xe5, 0xe7, 0xeb), 1));
            painter.drawLine(margin, y + rowH, margin + tableW, y + rowH);
            grandTotal += row.amount;
            y += rowH;
        }

        // --- Footer total ---
        y += 4;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x1b, 0x24, 0x40));
        painter.drawRect(margin, y, tableW, headerH);
        painter.setPen(QColor(0xee, 0xf2, 0xfb));
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 9, QFont::Bold));
        painter.drawText(QRect(margin + 6, y, tableW / 2, headerH), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Total: %1 transactions").arg(rows.size()));
        painter.drawText(QRect(margin + tableW / 2, y, tableW / 2 - 6, headerH), Qt::AlignRight | Qt::AlignVCenter,
            formatMoney(grandTotal));

        painter.end();
        emit toast(QStringLiteral("PDF report exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Party balance lookup ------------------------------------------------

QVariantMap QmlBackend::partyBalance(const QString& partyName) {
    try {
        if (partyName.trimmed().isEmpty()) {
            return QVariantMap();
        }
        // Get ledger with full date range (no filter = all transactions)
        domain::ReportRange range;
        range.start = QDate(2000, 1, 1);
        range.end = QDate::currentDate();
        range.days = 0;
        const QJsonObject ledger = context_.services().reports->ledger(partyName, range);
        const QJsonArray data = ledger.value(QStringLiteral("data")).toArray();

        double balance = 0.0;
        QString lastTxnDate;
        QString lastTxnType;
        double lastTxnAmount = 0.0;

        if (!data.isEmpty()) {
            const QJsonObject lastRow = data.last().toObject();
            balance = lastRow.value(QStringLiteral("balance")).toDouble();
            lastTxnDate = lastRow.value(QStringLiteral("date")).toString();
            lastTxnType = lastRow.value(QStringLiteral("type")).toString();
            lastTxnAmount = lastRow.value(QStringLiteral("amount")).toDouble();
        }

        QVariantMap result;
        result.insert(QStringLiteral("balance"), balance);
        result.insert(QStringLiteral("balanceLabel"), balance >= 0
            ? formatMoney(balance) + QStringLiteral(" Dr")
            : formatMoney(qAbs(balance)) + QStringLiteral(" Cr"));
        result.insert(QStringLiteral("lastDate"), lastTxnDate);
        result.insert(QStringLiteral("lastType"), lastTxnType);
        result.insert(QStringLiteral("lastAmount"), lastTxnAmount);
        result.insert(QStringLiteral("hasData"), !data.isEmpty());
        return result;
    } catch (...) {
        return QVariantMap();
    }
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
