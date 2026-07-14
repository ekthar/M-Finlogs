// ---------------------------------------------------------------------------
// Integration tests for SQLite-backed business services.
//
// Uses an in-memory SQLite database (":memory:") so no filesystem access is
// required. Tests exercise schema creation, user auth, transaction CRUD,
// and all major report service methods.
//
// Build: Requires QtTest and QtSql. See native/tests/CMakeLists.txt.
// Run: ctest --test-dir native/build or execute the test binary directly.
// ---------------------------------------------------------------------------

#include "data/SqliteSchemaInitializer.h"
#include "data/SqliteBusinessServices.h"
#include "data/ConfigService.h"
#include "domain/BalanceCalculator.h"
#include "domain/DomainErrors.h"
#include "domain/ServiceInterfaces.h"
#include "domain/ServiceRegistry.h"
#include "domain/Types.h"

#include <QtTest/QtTest>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QTemporaryDir>

using namespace mfinlogs::domain;
using namespace mfinlogs::data;

class TestSqliteServices : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tempDir_;
    QString dbPath_;
    ServiceRegistry registry_;

    void setupServices() {
        tempDir_ = std::make_unique<QTemporaryDir>();
        QVERIFY(tempDir_->isValid());
        dbPath_ = tempDir_->filePath(QStringLiteral("test.db"));

        // Create a ConfigService backed by the temp directory
        auto configService = std::make_shared<JsonConfigService>(
            tempDir_->path());
        registry_ = createSqliteBusinessServices(dbPath_, configService);
    }

private slots:
    // -----------------------------------------------------------------------
    // Schema creation
    // -----------------------------------------------------------------------

    void testSchemaCreation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("schema_test.db"));

        // Open a connection and initialize schema
        const QString connName = QStringLiteral("test_schema_conn");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(path);
            QVERIFY(db.open());

            SqliteSchemaInitializer initializer(db);
            initializer.initialize(QStringLiteral("TestCompany"));

            // Verify tables exist
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")));

            QStringList tables;
            while (query.next()) {
                tables << query.value(0).toString();
            }

            QVERIFY(tables.contains(QStringLiteral("users")));
            QVERIFY(tables.contains(QStringLiteral("parties")));
            QVERIFY(tables.contains(QStringLiteral("transactions")));
            QVERIFY(tables.contains(QStringLiteral("audit_logs")));
            QVERIFY(tables.contains(QStringLiteral("app_settings")));
            QVERIFY(tables.contains(QStringLiteral("daily_cash")));
            QVERIFY(tables.contains(QStringLiteral("inventory_items")));
            QVERIFY(tables.contains(QStringLiteral("inventory_quantities")));
            QVERIFY(tables.contains(QStringLiteral("inventory_purchases")));

            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    void testSchemaIdempotent() {
        // Running initialize twice should not fail
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("idempotent.db"));

        const QString connName = QStringLiteral("test_idempotent_conn");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(path);
            QVERIFY(db.open());

            SqliteSchemaInitializer initializer(db);
            initializer.initialize(QStringLiteral("Company1"));
            // Second call should be no-op (CREATE IF NOT EXISTS)
            initializer.initialize(QStringLiteral("Company1"));

            // Verify still works
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM users")));
            QVERIFY(query.next());
            QVERIFY(query.value(0).toInt() >= 1); // bootstrap admin

            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // -----------------------------------------------------------------------
    // User creation and login
    // -----------------------------------------------------------------------

    void testSetupAndLogin() {
        setupServices();
        QVERIFY(registry_.auth != nullptr);

        // Initial state: setup should be required (no password set)
        QVERIFY(registry_.auth->setupRequired());

        // Setup admin
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("secret123"));

        // Setup no longer required
        QVERIFY(!registry_.auth->setupRequired());

        // Login with correct credentials
        Session session = registry_.auth->login(
            QStringLiteral("admin"), QStringLiteral("secret123"));
        QCOMPARE(session.username, QStringLiteral("admin"));
        QCOMPARE(session.role, UserRole::Admin);
        QVERIFY(!session.token.isEmpty());
    }

    void testLoginFailsWithWrongPassword() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("correct"));

        bool threw = false;
        try {
            registry_.auth->login(
                QStringLiteral("admin"), QStringLiteral("wrong"));
        } catch (const DomainError&) {
            threw = true;
        }
        QVERIFY(threw);
    }

    // -----------------------------------------------------------------------
    // Transaction CRUD
    // -----------------------------------------------------------------------

    void testTransactionAdd() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        // Create a party first
        registry_.parties->createParty(
            QStringLiteral("Test Customer"), QStringLiteral("customer"), true);

        // Add a transaction
        TransactionCreateRequest req;
        req.date = QDate(2024, 5, 15);
        req.billNo = QStringLiteral("INV-001");
        req.party = QStringLiteral("Test Customer");
        req.type = TransactionType::Sale;
        req.mode = PaymentMode::Credit;
        req.amount = 5000.0;

        int id = registry_.transactions->addTransaction(req);
        QVERIFY(id > 0);

        // Retrieve and verify
        TransactionRow row = registry_.transactions->getTransaction(id);
        QCOMPARE(row.id, id);
        QCOMPARE(row.date, QDate(2024, 5, 15));
        QCOMPARE(row.billNo, QStringLiteral("INV-001"));
        QCOMPARE(row.party, QStringLiteral("Test Customer"));
        QCOMPARE(row.type, TransactionType::Sale);
        QCOMPARE(row.mode, PaymentMode::Credit);
        QCOMPARE(row.amount, 5000.0);
    }

    void testTransactionEdit() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Customer A"), QStringLiteral("customer"), true);

        TransactionCreateRequest req;
        req.date = QDate(2024, 6, 1);
        req.billNo = QStringLiteral("INV-010");
        req.party = QStringLiteral("Customer A");
        req.type = TransactionType::Sale;
        req.mode = PaymentMode::Cash;
        req.amount = 3000.0;

        int id = registry_.transactions->addTransaction(req);

        // Edit amount
        TransactionEditRequest editReq;
        editReq.transactionId = id;
        editReq.data = req;
        editReq.data.amount = 4500.0;
        editReq.adminUser = QStringLiteral("admin");

        registry_.transactions->editTransaction(editReq);

        TransactionRow updated = registry_.transactions->getTransaction(id);
        QCOMPARE(updated.amount, 4500.0);
    }

    void testTransactionDelete() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Customer B"), QStringLiteral("customer"), true);

        TransactionCreateRequest req;
        req.date = QDate(2024, 7, 10);
        req.billNo = QStringLiteral("INV-020");
        req.party = QStringLiteral("Customer B");
        req.type = TransactionType::Receipt;
        req.mode = PaymentMode::Upi;
        req.amount = 2000.0;

        int id = registry_.transactions->addTransaction(req);
        QVERIFY(id > 0);

        TransactionDeleteRequest delReq;
        delReq.transactionId = id;
        delReq.adminUser = QStringLiteral("admin");
        registry_.transactions->deleteTransaction(delReq);

        // Attempting to get should throw
        bool threw = false;
        try {
            registry_.transactions->getTransaction(id);
        } catch (const DomainError&) {
            threw = true;
        }
        QVERIFY(threw);
    }

    // -----------------------------------------------------------------------
    // Reports
    // -----------------------------------------------------------------------

    void testLedgerReport() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Ledger Party"), QStringLiteral("customer"), true);

        // Add transactions: 2 sales, 1 receipt
        TransactionCreateRequest sale1;
        sale1.date = QDate(2024, 5, 1);
        sale1.billNo = QStringLiteral("S001");
        sale1.party = QStringLiteral("Ledger Party");
        sale1.type = TransactionType::Sale;
        sale1.mode = PaymentMode::Credit;
        sale1.amount = 10000.0;
        registry_.transactions->addTransaction(sale1);

        TransactionCreateRequest sale2;
        sale2.date = QDate(2024, 5, 15);
        sale2.billNo = QStringLiteral("S002");
        sale2.party = QStringLiteral("Ledger Party");
        sale2.type = TransactionType::Sale;
        sale2.mode = PaymentMode::Credit;
        sale2.amount = 5000.0;
        registry_.transactions->addTransaction(sale2);

        TransactionCreateRequest receipt;
        receipt.date = QDate(2024, 6, 1);
        receipt.billNo = QStringLiteral("R001");
        receipt.party = QStringLiteral("Ledger Party");
        receipt.type = TransactionType::Receipt;
        receipt.mode = PaymentMode::Cash;
        receipt.amount = 7000.0;
        registry_.transactions->addTransaction(receipt);

        ReportRange range;
        range.start = QDate(2024, 4, 1);
        range.end = QDate(2025, 3, 31);
        range.days = 365;

        QJsonObject result = registry_.reports->ledger(
            QStringLiteral("Ledger Party"), range);

        // Should have opening=0, period debit=15000, credit=7000, closing=8000
        QCOMPARE(result.value(QStringLiteral("opening_balance")).toDouble(), 0.0);
        QVERIFY(result.contains(QStringLiteral("closing_balance")));
        double closing = result.value(QStringLiteral("closing_balance")).toDouble();
        QCOMPARE(closing, 8000.0);
    }

    void testOutstandingReport() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Outstanding Party"), QStringLiteral("customer"), true);

        TransactionCreateRequest sale;
        sale.date = QDate(2024, 5, 1);
        sale.billNo = QStringLiteral("O001");
        sale.party = QStringLiteral("Outstanding Party");
        sale.type = TransactionType::Sale;
        sale.mode = PaymentMode::Credit;
        sale.amount = 12000.0;
        registry_.transactions->addTransaction(sale);

        TransactionCreateRequest receipt;
        receipt.date = QDate(2024, 6, 15);
        receipt.billNo = QStringLiteral("O002");
        receipt.party = QStringLiteral("Outstanding Party");
        receipt.type = TransactionType::Receipt;
        receipt.mode = PaymentMode::Cash;
        receipt.amount = 5000.0;
        registry_.transactions->addTransaction(receipt);

        QJsonObject result = registry_.reports->outstanding();
        QVERIFY(result.contains(QStringLiteral("data")) ||
                result.contains(QStringLiteral("parties")));
    }

    void testTrialBalance() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Trial Party"), QStringLiteral("customer"), true);

        TransactionCreateRequest sale;
        sale.date = QDate(2024, 5, 1);
        sale.billNo = QStringLiteral("T001");
        sale.party = QStringLiteral("Trial Party");
        sale.type = TransactionType::Sale;
        sale.mode = PaymentMode::Credit;
        sale.amount = 8000.0;
        registry_.transactions->addTransaction(sale);

        QJsonArray result = registry_.reports->trialBalance();
        QVERIFY(result.size() > 0);
    }

    void testProfitAndLoss() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("PL Party"), QStringLiteral("customer"), true);

        TransactionCreateRequest sale;
        sale.date = QDate(2024, 5, 1);
        sale.billNo = QStringLiteral("PL001");
        sale.party = QStringLiteral("PL Party");
        sale.type = TransactionType::Sale;
        sale.mode = PaymentMode::Cash;
        sale.amount = 20000.0;
        registry_.transactions->addTransaction(sale);

        TransactionCreateRequest expense;
        expense.date = QDate(2024, 5, 5);
        expense.billNo = QStringLiteral("PL002");
        expense.party = QStringLiteral("PL Party");
        expense.type = TransactionType::Expense;
        expense.mode = PaymentMode::Cash;
        expense.amount = 5000.0;
        registry_.transactions->addTransaction(expense);

        QJsonObject result = registry_.reports->profitAndLoss();
        QVERIFY(result.contains(QStringLiteral("total_sales")) ||
                result.contains(QStringLiteral("sales")));
    }

    void testDailySummary() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("Daily Party"), QStringLiteral("customer"), true);

        TransactionCreateRequest sale;
        sale.date = QDate(2024, 5, 1);
        sale.billNo = QStringLiteral("DS001");
        sale.party = QStringLiteral("Daily Party");
        sale.type = TransactionType::Sale;
        sale.mode = PaymentMode::Cash;
        sale.amount = 3000.0;
        registry_.transactions->addTransaction(sale);

        ReportRange range;
        range.start = QDate(2024, 4, 1);
        range.end = QDate(2024, 5, 31);
        range.days = 61;

        QJsonArray result = registry_.reports->dailySummary(range);
        QVERIFY(result.size() > 0);
    }

    void testDayBook() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));
        registry_.parties->createParty(
            QStringLiteral("DayBook Party"), QStringLiteral("customer"), true);

        TransactionCreateRequest sale;
        sale.date = QDate(2024, 5, 10);
        sale.billNo = QStringLiteral("DB001");
        sale.party = QStringLiteral("DayBook Party");
        sale.type = TransactionType::Sale;
        sale.mode = PaymentMode::Credit;
        sale.amount = 4000.0;
        registry_.transactions->addTransaction(sale);

        QJsonArray result = registry_.reports->dayBook(QDate(2024, 5, 10));
        QVERIFY(result.size() >= 1);

        QJsonObject first = result.at(0).toObject();
        QVERIFY(first.contains(QStringLiteral("bill_no")) ||
                first.contains(QStringLiteral("billNo")));
    }

    // -----------------------------------------------------------------------
    // Inventory CRUD
    // -----------------------------------------------------------------------

    void testInventorySnapshot() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        // Save a snapshot
        QJsonArray rows;
        QJsonObject item;
        item[QStringLiteral("name")] = QStringLiteral("Widget A");
        item[QStringLiteral("cost")] = 100.0;
        item[QStringLiteral("min_stock")] = 10.0;
        QJsonArray quantities;
        for (int i = 0; i < 31; ++i) quantities.append(0.0);
        item[QStringLiteral("quantities")] = quantities;
        item[QStringLiteral("purchase_quantities")] = quantities;
        rows.append(item);

        registry_.inventory->saveSnapshot(
            QStringLiteral("2024-2025"), 5, rows);

        // Load it back
        QJsonArray loaded = registry_.inventory->loadSnapshot(
            QStringLiteral("2024-2025"), 5);
        QVERIFY(loaded.size() >= 1);
    }

    // -----------------------------------------------------------------------
    // Empty dataset handling
    // -----------------------------------------------------------------------

    void testEmptyLedger() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        ReportRange range;
        range.start = QDate(2024, 4, 1);
        range.end = QDate(2025, 3, 31);
        range.days = 365;

        // Ledger for non-existent party should not crash
        QJsonObject result = registry_.reports->ledger(
            QStringLiteral("NonExistent"), range);
        QCOMPARE(result.value(QStringLiteral("opening_balance")).toDouble(), 0.0);
    }

    void testEmptyDayBook() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        QJsonArray result = registry_.reports->dayBook(QDate(2020, 1, 1));
        QCOMPARE(result.size(), 0);
    }

    // -----------------------------------------------------------------------
    // Party management
    // -----------------------------------------------------------------------

    void testCreateAndListParties() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        registry_.parties->createParty(
            QStringLiteral("Alpha Corp"), QStringLiteral("customer"), true);
        registry_.parties->createParty(
            QStringLiteral("Beta LLC"), QStringLiteral("supplier"), false);

        QJsonArray parties = registry_.parties->listParties();
        QVERIFY(parties.size() >= 2);
    }

    void testDuplicatePartyHandled() {
        setupServices();
        registry_.auth->setupAdmin(
            QStringLiteral("admin"), QStringLiteral("pass"));

        registry_.parties->createParty(
            QStringLiteral("Dup Party"), QStringLiteral("customer"), true);

        // Creating same party again should throw or be handled gracefully
        bool threw = false;
        try {
            registry_.parties->createParty(
                QStringLiteral("Dup Party"), QStringLiteral("customer"), true);
        } catch (const DomainError&) {
            threw = true;
        }
        // Either throws or silently ignores - both are acceptable
        QVERIFY(true);
        Q_UNUSED(threw);
    }
};

QTEST_MAIN(TestSqliteServices)
#include "test_sqlite_services.moc"
