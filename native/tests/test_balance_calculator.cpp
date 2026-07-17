// ---------------------------------------------------------------------------
// Unit tests for BalanceCalculator - the canonical party balance computation.
//
// These tests verify correctness of balance calculation logic using pure
// in-memory data (no database required). They cover edge cases that could
// cause financial discrepancies between reports.
//
// Build: Requires QtTest. Add to CMakeLists.txt as a test target.
// Run: ctest --test-dir native/build or directly execute the test binary.
// ---------------------------------------------------------------------------

#include "domain/BalanceCalculator.h"

#include <QtTest/QtTest>
#include <QDate>
#include <QString>
#include <QVector>

using namespace mfinlogs::domain;

class TestBalanceCalculator : public QObject {
    Q_OBJECT

private slots:
    // -----------------------------------------------------------------------
    // Transaction type classification
    // -----------------------------------------------------------------------

    void testIsDebitType() {
        QVERIFY(isDebitType(QStringLiteral("SALE")));
        QVERIFY(!isDebitType(QStringLiteral("RECEIPT")));
        QVERIFY(!isDebitType(QStringLiteral("EXPENSE")));
        QVERIFY(!isDebitType(QStringLiteral("PURCHASE")));
        QVERIFY(!isDebitType(QStringLiteral("SALE RETURN")));
    }

    void testIsCreditType() {
        QVERIFY(isCreditType(QStringLiteral("RECEIPT")));
        QVERIFY(isCreditType(QStringLiteral("RECIEPT")));
        QVERIFY(isCreditType(QStringLiteral("SALE RETURN")));
        QVERIFY(isCreditType(QStringLiteral("SALES RETURN")));
        QVERIFY(isCreditType(QStringLiteral("RETURN")));
        QVERIFY(!isCreditType(QStringLiteral("SALE")));
        QVERIFY(!isCreditType(QStringLiteral("EXPENSE")));
        QVERIFY(!isCreditType(QStringLiteral("PURCHASE")));
    }

    void testAffectsPartyBalance() {
        QVERIFY(affectsPartyBalance(QStringLiteral("SALE")));
        QVERIFY(affectsPartyBalance(QStringLiteral("RECEIPT")));
        QVERIFY(affectsPartyBalance(QStringLiteral("SALE RETURN")));
        // Expense and Purchase do NOT affect party receivable balance
        QVERIFY(!affectsPartyBalance(QStringLiteral("EXPENSE")));
        QVERIFY(!affectsPartyBalance(QStringLiteral("PURCHASE")));
    }

    void testNormalizeTransactionType() {
        QCOMPARE(normalizeTransactionType(QStringLiteral("  Sale  ")), QStringLiteral("SALE"));
        QCOMPARE(normalizeTransactionType(QStringLiteral("sale return")), QStringLiteral("SALE RETURN"));
        QCOMPARE(normalizeTransactionType(QStringLiteral("Receipt")), QStringLiteral("RECEIPT"));
    }

    // -----------------------------------------------------------------------
    // Balance delta
    // -----------------------------------------------------------------------

    void testBalanceDelta_sale() {
        QCOMPARE(balanceDelta(QStringLiteral("SALE"), 1000.0), 1000.0);
    }

    void testBalanceDelta_receipt() {
        QCOMPARE(balanceDelta(QStringLiteral("RECEIPT"), 500.0), -500.0);
    }

    void testBalanceDelta_saleReturn() {
        QCOMPARE(balanceDelta(QStringLiteral("SALE RETURN"), 200.0), -200.0);
    }

    void testBalanceDelta_expense() {
        // Expense does NOT affect party balance
        QCOMPARE(balanceDelta(QStringLiteral("EXPENSE"), 300.0), 0.0);
    }

    void testBalanceDelta_purchase() {
        // Purchase does NOT affect party balance
        QCOMPARE(balanceDelta(QStringLiteral("PURCHASE"), 400.0), 0.0);
    }

    // -----------------------------------------------------------------------
    // computeBalance - fixture tests
    // -----------------------------------------------------------------------

    void testComputeBalance_zeroTransactions() {
        QVector<BalanceTransaction> transactions;
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.openingBalance, 0.0);
        QCOMPARE(summary.periodDebit, 0.0);
        QCOMPARE(summary.periodCredit, 0.0);
        QCOMPARE(summary.closingBalance, 0.0);
        QCOMPARE(summary.transactionCount, 0);
        QVERIFY(!summary.lastTransactionDate.isValid());
    }

    void testComputeBalance_onlyOpeningTransactions() {
        // All transactions are before the period start
        QVector<BalanceTransaction> transactions = {
            {QDate(2023, 6, 15), QStringLiteral("SALE"), 5000.0},
            {QDate(2023, 9, 20), QStringLiteral("RECEIPT"), 2000.0},
            {QDate(2024, 1, 10), QStringLiteral("SALE"), 3000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        // Opening = 5000 - 2000 + 3000 = 6000
        QCOMPARE(summary.openingBalance, 6000.0);
        QCOMPARE(summary.periodDebit, 0.0);
        QCOMPARE(summary.periodCredit, 0.0);
        QCOMPARE(summary.closingBalance, 6000.0);
        QCOMPARE(summary.transactionCount, 0);
    }

    void testComputeBalance_transactionsBeforeFinancialYear() {
        // Verify that historical transactions (before FY start) contribute
        // to opening balance, even if they span multiple financial years.
        QVector<BalanceTransaction> transactions = {
            {QDate(2020, 5, 1), QStringLiteral("SALE"), 10000.0},   // FY 2020-2021
            {QDate(2021, 7, 15), QStringLiteral("RECEIPT"), 3000.0}, // FY 2021-2022
            {QDate(2022, 12, 1), QStringLiteral("SALE"), 2000.0},   // FY 2022-2023
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 1000.0},    // In period
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        // Opening = 10000 - 3000 + 2000 = 9000 (all before April 2024)
        QCOMPARE(summary.openingBalance, 9000.0);
        QCOMPARE(summary.periodDebit, 1000.0);
        QCOMPARE(summary.periodCredit, 0.0);
        QCOMPARE(summary.closingBalance, 10000.0);
        QCOMPARE(summary.transactionCount, 1);
    }

    void testComputeBalance_transactionsInsideFinancialYear() {
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 4, 5), QStringLiteral("SALE"), 5000.0},
            {QDate(2024, 6, 10), QStringLiteral("RECEIPT"), 2000.0},
            {QDate(2024, 9, 15), QStringLiteral("SALE"), 3000.0},
            {QDate(2025, 1, 20), QStringLiteral("SALE RETURN"), 500.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.openingBalance, 0.0);
        QCOMPARE(summary.periodDebit, 8000.0);  // 5000 + 3000
        QCOMPARE(summary.periodCredit, 2500.0); // 2000 + 500
        QCOMPARE(summary.closingBalance, 5500.0);
        QCOMPARE(summary.transactionCount, 4);
        QCOMPARE(summary.lastTransactionDate, QDate(2025, 1, 20));
    }

    void testComputeBalance_debitGreaterThanCredit() {
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 10000.0},
            {QDate(2024, 6, 1), QStringLiteral("RECEIPT"), 3000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.closingBalance, 7000.0); // Positive = Dr
        QVERIFY(summary.closingBalance > 0.0);
    }

    void testComputeBalance_creditGreaterThanDebit() {
        // Overpayment scenario
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 1000.0},
            {QDate(2024, 6, 1), QStringLiteral("RECEIPT"), 5000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.closingBalance, -4000.0); // Negative = Cr (we owe)
        QVERIFY(summary.closingBalance < 0.0);
    }

    void testComputeBalance_returns() {
        // Various return types all reduce the balance
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 10000.0},
            {QDate(2024, 5, 5), QStringLiteral("SALE RETURN"), 1000.0},
            {QDate(2024, 5, 10), QStringLiteral("SALES RETURN"), 500.0},
            {QDate(2024, 5, 15), QStringLiteral("RETURN"), 200.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.periodDebit, 10000.0);
        QCOMPARE(summary.periodCredit, 1700.0);  // 1000 + 500 + 200
        QCOMPARE(summary.closingBalance, 8300.0);
    }

    void testComputeBalance_multipleParties() {
        // Each party should be calculated independently
        // Party A transactions
        QVector<BalanceTransaction> partyA = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 5000.0},
            {QDate(2024, 6, 1), QStringLiteral("RECEIPT"), 2000.0},
        };
        // Party B transactions
        QVector<BalanceTransaction> partyB = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 8000.0},
            {QDate(2024, 7, 1), QStringLiteral("RECEIPT"), 8000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summaryA = computeBalance(partyA, start, end);
        BalanceSummary summaryB = computeBalance(partyB, start, end);

        QCOMPARE(summaryA.closingBalance, 3000.0);
        QCOMPARE(summaryB.closingBalance, 0.0);
    }

    void testComputeBalance_sameDateTransactions() {
        // Multiple transactions on the same date
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 1000.0},
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 2000.0},
            {QDate(2024, 5, 1), QStringLiteral("RECEIPT"), 500.0},
            {QDate(2024, 5, 1), QStringLiteral("SALE RETURN"), 300.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.periodDebit, 3000.0);  // 1000 + 2000
        QCOMPARE(summary.periodCredit, 800.0);  // 500 + 300
        QCOMPARE(summary.closingBalance, 2200.0);
        QCOMPARE(summary.transactionCount, 4);
        QCOMPARE(summary.lastTransactionDate, QDate(2024, 5, 1));
    }

    void testComputeBalance_largeTransactionVolume() {
        QVector<BalanceTransaction> transactions;
        transactions.reserve(10000);
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        // Generate 5000 sales and 5000 receipts
        for (int i = 0; i < 5000; ++i) {
            const QDate date = start.addDays(i % 365);
            transactions.append({date, QStringLiteral("SALE"), 100.0});
            transactions.append({date, QStringLiteral("RECEIPT"), 80.0});
        }

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.periodDebit, 500000.0);  // 5000 * 100
        QCOMPARE(summary.periodCredit, 400000.0); // 5000 * 80
        QCOMPARE(summary.closingBalance, 100000.0);
        QCOMPARE(summary.transactionCount, 10000);
    }

    void testComputeBalance_expenseAndPurchaseIgnored() {
        // Expense and Purchase must NOT affect party balance
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 5000.0},
            {QDate(2024, 5, 2), QStringLiteral("EXPENSE"), 1000.0},
            {QDate(2024, 5, 3), QStringLiteral("PURCHASE"), 2000.0},
            {QDate(2024, 5, 4), QStringLiteral("RECEIPT"), 1000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        // Only Sale and Receipt affect balance: 5000 - 1000 = 4000
        QCOMPARE(summary.periodDebit, 5000.0);
        QCOMPARE(summary.periodCredit, 1000.0);
        QCOMPARE(summary.closingBalance, 4000.0);
        // Only 2 transactions affect balance, but all 4 have types checked
        // Actually, the function counts only transactions that affect balance
        QCOMPARE(summary.transactionCount, 2); // Only SALE and RECEIPT counted
    }

    void testComputeBalance_legacyMisspelling() {
        // Verify the legacy "RECIEPT" spelling is handled
        QVector<BalanceTransaction> transactions = {
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 5000.0},
            {QDate(2024, 6, 1), QStringLiteral("RECIEPT"), 2000.0},
        };
        const QDate start(2024, 4, 1);
        const QDate end(2025, 3, 31);

        BalanceSummary summary = computeBalance(transactions, start, end);

        QCOMPARE(summary.closingBalance, 3000.0);
        QCOMPARE(summary.periodCredit, 2000.0);
    }

    // -----------------------------------------------------------------------
    // computeOpeningBalance
    // -----------------------------------------------------------------------

    void testComputeOpeningBalance() {
        QVector<BalanceTransaction> transactions = {
            {QDate(2023, 1, 1), QStringLiteral("SALE"), 10000.0},
            {QDate(2023, 6, 1), QStringLiteral("RECEIPT"), 4000.0},
            {QDate(2024, 4, 5), QStringLiteral("SALE"), 2000.0}, // After cutoff
        };

        double opening = computeOpeningBalance(transactions, QDate(2024, 4, 1));

        // Only pre-April 2024: 10000 - 4000 = 6000
        QCOMPARE(opening, 6000.0);
    }

    // -----------------------------------------------------------------------
    // computeLifetimeBalance
    // -----------------------------------------------------------------------

    void testComputeLifetimeBalance() {
        QVector<BalanceTransaction> transactions = {
            {QDate(2020, 1, 1), QStringLiteral("SALE"), 10000.0},
            {QDate(2021, 6, 1), QStringLiteral("RECEIPT"), 3000.0},
            {QDate(2024, 5, 1), QStringLiteral("SALE"), 2000.0},
            {QDate(2024, 8, 1), QStringLiteral("SALE RETURN"), 500.0},
        };

        double lifetime = computeLifetimeBalance(transactions);

        // 10000 - 3000 + 2000 - 500 = 8500
        QCOMPARE(lifetime, 8500.0);
    }

    // -----------------------------------------------------------------------
    // SQL helper generation
    // -----------------------------------------------------------------------

    void testBalanceSumSql() {
        const QString sql = balanceSumSql(QStringLiteral("txn_type"), QStringLiteral("amount"));
        // Verify it contains the key patterns
        QVERIFY(sql.contains(QStringLiteral("SALE")));
        QVERIFY(sql.contains(QStringLiteral("RECEIPT")));
        QVERIFY(sql.contains(QStringLiteral("RECIEPT")));
        QVERIFY(sql.contains(QStringLiteral("SALE RETURN")));
        QVERIFY(sql.contains(QStringLiteral("SALES RETURN")));
        QVERIFY(sql.contains(QStringLiteral("RETURN")));
    }

    void testDebitSumSql() {
        const QString sql = debitSumSql(QStringLiteral("t.txn_type"), QStringLiteral("t.amount"));
        QVERIFY(sql.contains(QStringLiteral("SALE")));
        QVERIFY(!sql.contains(QStringLiteral("RECEIPT")));
    }

    void testCreditSumSql() {
        const QString sql = creditSumSql(QStringLiteral("t.txn_type"), QStringLiteral("t.amount"));
        QVERIFY(sql.contains(QStringLiteral("RECEIPT")));
        QVERIFY(sql.contains(QStringLiteral("RECIEPT")));
        QVERIFY(sql.contains(QStringLiteral("SALE RETURN")));
    }

    // -----------------------------------------------------------------------
    // isZeroBalance
    // -----------------------------------------------------------------------

    void testIsZeroBalance() {
        QVERIFY(isZeroBalance(0.0));
        QVERIFY(isZeroBalance(0.004));
        QVERIFY(isZeroBalance(-0.004));
        QVERIFY(!isZeroBalance(0.01));
        QVERIFY(!isZeroBalance(-0.01));
        QVERIFY(!isZeroBalance(100.0));
    }

    // -----------------------------------------------------------------------
    // BalanceSummary::periodMovement and recalculate
    // -----------------------------------------------------------------------

    void testBalanceSummary_periodMovement() {
        BalanceSummary s;
        s.periodDebit = 5000.0;
        s.periodCredit = 2000.0;
        QCOMPARE(s.periodMovement(), 3000.0);
    }

    void testBalanceSummary_recalculate() {
        BalanceSummary s;
        s.openingBalance = 1000.0;
        s.periodDebit = 5000.0;
        s.periodCredit = 2000.0;
        s.recalculate();
        QCOMPARE(s.closingBalance, 4000.0); // 1000 + 5000 - 2000
    }
};

QTEST_APPLESS_MAIN(TestBalanceCalculator)
#include "test_balance_calculator.moc"
