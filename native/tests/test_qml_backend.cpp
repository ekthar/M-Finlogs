// ---------------------------------------------------------------------------
// Unit tests for QmlBackend formatting and utility methods.
//
// Tests the pure functions that do not require a database connection:
// - formatMoney: locale-aware money formatting
// - formatDate: ISO to display format conversion
// - todayIso: returns today's date in ISO format
// - nextBillNumber: auto-increments bill number strings
//
// Build: Requires QtTest. See native/tests/CMakeLists.txt.
// Run: ctest --test-dir native/build or execute the test binary directly.
// ---------------------------------------------------------------------------

#include "app/QmlBackend.h"
#include "app/AppContext.h"

#include <QtTest/QtTest>
#include <QDate>
#include <QString>

using namespace mfinlogs::app;

class TestQmlBackend : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<AppContext> context_;
    std::unique_ptr<QmlBackend> backend_;

    void createBackend() {
        // AppContext with test-specific org/app names
        context_ = std::make_unique<AppContext>(
            QStringLiteral("MFinlogsTest"), QStringLiteral("test-backend"));
        backend_ = std::make_unique<QmlBackend>(*context_, nullptr);
    }

private slots:
    void initTestCase() {
        createBackend();
    }

    // -----------------------------------------------------------------------
    // formatMoney
    // -----------------------------------------------------------------------

    void testFormatMoney_zero() {
        const QString result = backend_->formatMoney(0.0);
        // Should contain "0" somewhere
        QVERIFY(result.contains(QStringLiteral("0")));
    }

    void testFormatMoney_positive() {
        const QString result = backend_->formatMoney(1234.56);
        // Should format with decimal places
        QVERIFY(result.contains(QStringLiteral("1")));
        QVERIFY(result.contains(QStringLiteral("234")) ||
                result.contains(QStringLiteral("1,234")));
    }

    void testFormatMoney_negative() {
        const QString result = backend_->formatMoney(-500.0);
        // Negative values should have a minus or brackets
        QVERIFY(result.contains(QStringLiteral("500")));
        QVERIFY(result.contains(QStringLiteral("-")) ||
                result.contains(QStringLiteral("(")));
    }

    void testFormatMoney_large() {
        const QString result = backend_->formatMoney(1234567.89);
        // Should handle large numbers with separators
        QVERIFY(result.contains(QStringLiteral("1234567")) ||
                result.contains(QStringLiteral("1,234,567")) ||
                result.contains(QStringLiteral("12,34,567")));  // Indian locale
    }

    void testFormatMoney_verySmall() {
        const QString result = backend_->formatMoney(0.01);
        QVERIFY(result.contains(QStringLiteral("0")) ||
                result.contains(QStringLiteral("01")));
    }

    // -----------------------------------------------------------------------
    // formatDate
    // -----------------------------------------------------------------------

    void testFormatDate_validIso() {
        const QString result = backend_->formatDate(QStringLiteral("2024-05-15"));
        // Should produce a readable date (not empty, not the raw ISO)
        QVERIFY(!result.isEmpty());
        // Should contain day, month info in some format
        QVERIFY(result.contains(QStringLiteral("15")) ||
                result.contains(QStringLiteral("May")) ||
                result.contains(QStringLiteral("05")));
    }

    void testFormatDate_emptyString() {
        const QString result = backend_->formatDate(QStringLiteral(""));
        // Should handle empty gracefully
        QVERIFY(result.isEmpty() || result == QStringLiteral("Invalid date"));
    }

    void testFormatDate_invalidDate() {
        const QString result = backend_->formatDate(QStringLiteral("not-a-date"));
        // Should not crash, returns empty or error string
        QVERIFY(!result.isNull());
    }

    // -----------------------------------------------------------------------
    // todayIso
    // -----------------------------------------------------------------------

    void testTodayIso_format() {
        const QString result = backend_->todayIso();
        // Should be in YYYY-MM-DD format
        QCOMPARE(result.length(), 10);
        QCOMPARE(result.at(4), QChar('-'));
        QCOMPARE(result.at(7), QChar('-'));
    }

    void testTodayIso_matchesCurrent() {
        const QString result = backend_->todayIso();
        const QString expected = QDate::currentDate().toString(Qt::ISODate);
        QCOMPARE(result, expected);
    }

    // -----------------------------------------------------------------------
    // nextBillNumber
    // -----------------------------------------------------------------------

    void testNextBillNumber_simpleNumeric() {
        const QString result = backend_->nextBillNumber(QStringLiteral("100"));
        QCOMPARE(result, QStringLiteral("101"));
    }

    void testNextBillNumber_withPrefix() {
        const QString result = backend_->nextBillNumber(QStringLiteral("INV-099"));
        // Should increment the numeric suffix
        QCOMPARE(result, QStringLiteral("INV-100"));
    }

    void testNextBillNumber_empty() {
        const QString result = backend_->nextBillNumber(QStringLiteral(""));
        // Should handle empty gracefully - return "1" or empty
        QVERIFY(result == QStringLiteral("1") || result.isEmpty() ||
                result == QStringLiteral("0") || result == QStringLiteral("01"));
    }

    void testNextBillNumber_noNumericSuffix() {
        const QString result = backend_->nextBillNumber(QStringLiteral("ABC"));
        // When there is no numeric part, behavior is implementation-defined
        // but should not crash
        QVERIFY(!result.isNull());
    }

    void testNextBillNumber_leadingZeros() {
        const QString result = backend_->nextBillNumber(QStringLiteral("INV-009"));
        // Should preserve leading zeros or increment correctly
        QVERIFY(result == QStringLiteral("INV-010") ||
                result == QStringLiteral("INV-10"));
    }

    // -----------------------------------------------------------------------
    // Property accessors (no auth state)
    // -----------------------------------------------------------------------

    void testAuthenticated_default() {
        QCOMPARE(backend_->authenticated(), false);
    }

    void testCurrentUser_default() {
        QVERIFY(backend_->currentUser().isEmpty());
    }

    void testCurrentRole_default() {
        QVERIFY(backend_->currentRole().isEmpty());
    }

    void testIsAdmin_default() {
        QCOMPARE(backend_->isAdmin(), false);
    }

    void testLoadingStates_default() {
        QVariantMap states = backend_->loadingStates();
        // All loading states should be false by default
        for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
            QCOMPARE(it.value().toBool(), false);
        }
    }
};

QTEST_MAIN(TestQmlBackend)
#include "test_qml_backend.moc"
