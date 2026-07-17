#pragma once

// ---------------------------------------------------------------------------
// BalanceCalculator - Canonical party balance computation for M-Finlogs.
//
// This header provides the single source of truth for determining how
// transaction types affect a customer's receivable (party) balance.
//
// Rules (matching Electron backend.py):
//   - SALE                 => debit  (increases receivable)
//   - RECEIPT / RECIEPT    => credit (decreases receivable)
//   - SALE RETURN / SALES RETURN / RETURN => credit (decreases receivable)
//   - EXPENSE / PURCHASE   => NO EFFECT on party receivable balance
//
// Sign convention:
//   Positive balance = party owes us (Dr / debit balance)
//   Negative balance = we owe the party (Cr / credit balance)
//
// All report methods (ledger, outstanding, trial balance, credit followup,
// closing report, export) MUST use these functions to ensure consistency.
// ---------------------------------------------------------------------------

#include <QString>
#include <QVector>
#include <QDate>

#include <algorithm>
#include <cmath>

namespace mfinlogs::domain {

// ---------------------------------------------------------------------------
// Transaction classification
// ---------------------------------------------------------------------------

/// Returns true if the normalized (trimmed, uppercased) transaction type
/// represents a debit to the party's receivable account.
inline bool isDebitType(const QString& normalizedType) {
    return normalizedType == QStringLiteral("SALE");
}

/// Returns true if the normalized (trimmed, uppercased) transaction type
/// represents a credit to the party's receivable account.
inline bool isCreditType(const QString& normalizedType) {
    return normalizedType == QStringLiteral("RECEIPT")
        || normalizedType == QStringLiteral("RECIEPT")
        || normalizedType == QStringLiteral("SALE RETURN")
        || normalizedType == QStringLiteral("SALES RETURN")
        || normalizedType == QStringLiteral("RETURN");
}

/// Returns true if the transaction type affects party receivable balance.
inline bool affectsPartyBalance(const QString& normalizedType) {
    return isDebitType(normalizedType) || isCreditType(normalizedType);
}

/// Normalize a raw transaction type string for comparison.
inline QString normalizeTransactionType(const QString& raw) {
    return raw.trimmed().toUpper();
}

// ---------------------------------------------------------------------------
// Balance delta computation
// ---------------------------------------------------------------------------

/// Compute the signed delta that a single transaction applies to the party
/// receivable balance. Returns +amount for debits, -amount for credits, and
/// 0.0 for transaction types that do not affect party balance.
inline double balanceDelta(const QString& normalizedType, double amount) {
    if (isDebitType(normalizedType)) {
        return amount;
    }
    if (isCreditType(normalizedType)) {
        return -amount;
    }
    return 0.0;
}

// ---------------------------------------------------------------------------
// Aggregate balance result
// ---------------------------------------------------------------------------

/// Holds the computed balance summary for a party over a given period.
struct BalanceSummary {
    double openingBalance  = 0.0; ///< Cumulative balance before period start
    double periodDebit     = 0.0; ///< Total debits within the period
    double periodCredit    = 0.0; ///< Total credits within the period
    double closingBalance  = 0.0; ///< opening + periodDebit - periodCredit
    int    transactionCount = 0;  ///< Number of transactions in the period
    QDate  lastTransactionDate;   ///< Most recent transaction date in the period

    /// Net movement within the period (periodDebit - periodCredit)
    double periodMovement() const { return periodDebit - periodCredit; }

    /// Recalculate closingBalance from opening + period movement
    void recalculate() { closingBalance = openingBalance + periodDebit - periodCredit; }
};

// ---------------------------------------------------------------------------
// Transaction input for batch calculation
// ---------------------------------------------------------------------------

/// Minimal transaction representation for balance calculation.
struct BalanceTransaction {
    QDate   date;
    QString normalizedType; ///< Already trimmed and uppercased
    double  amount = 0.0;
};

// ---------------------------------------------------------------------------
// Batch calculation
// ---------------------------------------------------------------------------

/// Compute a full BalanceSummary from a set of transactions.
/// @param transactions  All transactions for the party (any time range)
/// @param periodStart   First date of the reporting period (inclusive)
/// @param periodEnd     Last date of the reporting period (inclusive)
///
/// Transactions before periodStart contribute to openingBalance.
/// Transactions between periodStart and periodEnd (inclusive) contribute to
/// periodDebit/periodCredit and transactionCount.
/// Transactions after periodEnd are ignored.
///
/// This is the CANONICAL balance calculation - all reports must use this or
/// the equivalent SQL patterns that match this logic.
inline BalanceSummary computeBalance(
    const QVector<BalanceTransaction>& transactions,
    const QDate& periodStart,
    const QDate& periodEnd)
{
    BalanceSummary summary;

    for (const BalanceTransaction& txn : transactions) {
        if (!affectsPartyBalance(txn.normalizedType)) {
            continue;
        }

        if (txn.date < periodStart) {
            // Historical: contributes to opening balance
            summary.openingBalance += balanceDelta(txn.normalizedType, txn.amount);
        } else if (txn.date <= periodEnd) {
            // Period transaction
            if (isDebitType(txn.normalizedType)) {
                summary.periodDebit += txn.amount;
            } else {
                summary.periodCredit += txn.amount;
            }
            summary.transactionCount++;
            if (!summary.lastTransactionDate.isValid() || txn.date > summary.lastTransactionDate) {
                summary.lastTransactionDate = txn.date;
            }
        }
        // Transactions after periodEnd are ignored
    }

    summary.recalculate();
    return summary;
}

/// Compute opening balance only (all transactions before a given date).
/// Equivalent to SUM(Sale) - SUM(Receipt|SaleReturn) for txn_date < cutoff.
inline double computeOpeningBalance(
    const QVector<BalanceTransaction>& transactions,
    const QDate& cutoffDate)
{
    double balance = 0.0;
    for (const BalanceTransaction& txn : transactions) {
        if (txn.date >= cutoffDate) continue;
        balance += balanceDelta(txn.normalizedType, txn.amount);
    }
    return balance;
}

/// Compute lifetime closing balance (all transactions, no date filter).
/// Equivalent to SUM(Sale) - SUM(Receipt|SaleReturn) across all time.
inline double computeLifetimeBalance(
    const QVector<BalanceTransaction>& transactions)
{
    double balance = 0.0;
    for (const BalanceTransaction& txn : transactions) {
        balance += balanceDelta(txn.normalizedType, txn.amount);
    }
    return balance;
}

// ---------------------------------------------------------------------------
// SQL WHERE clause helpers (for embedding in queries)
// ---------------------------------------------------------------------------

/// The canonical SQL CASE expression for computing party balance from a
/// result set. Use in SELECT SUM(...) queries.
/// Column name for the type field and amount field are parameters.
///
/// Example usage:
///   QString sql = "SELECT " + balanceSumSql("txn_type", "amount") + " FROM ...";
///
/// This ensures all SQL queries use the same type matching logic.
inline QString balanceSumSql(const QString& typeCol, const QString& amountCol) {
    return QStringLiteral(
        "SUM(CASE "
        "WHEN UPPER(TRIM(%1))='SALE' THEN %2 "
        "WHEN UPPER(TRIM(%1)) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') THEN -%2 "
        "ELSE 0 END)"
    ).arg(typeCol, amountCol);
}

/// SQL CASE for debit total only (Sale transactions).
inline QString debitSumSql(const QString& typeCol, const QString& amountCol) {
    return QStringLiteral(
        "SUM(CASE WHEN UPPER(TRIM(%1))='SALE' THEN %2 ELSE 0 END)"
    ).arg(typeCol, amountCol);
}

/// SQL CASE for credit total only (Receipt, Sale Return, etc).
inline QString creditSumSql(const QString& typeCol, const QString& amountCol) {
    return QStringLiteral(
        "SUM(CASE WHEN UPPER(TRIM(%1)) IN ('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN') "
        "THEN %2 ELSE 0 END)"
    ).arg(typeCol, amountCol);
}

/// The IN clause for credit transaction types (for WHERE filters).
inline QString creditTypesInClause() {
    return QStringLiteral("('RECEIPT','RECIEPT','SALE RETURN','SALES RETURN','RETURN')");
}

/// Check if a balance is effectively zero (within rounding tolerance).
inline bool isZeroBalance(double balance, double tolerance = 0.005) {
    return std::abs(balance) < tolerance;
}

} // namespace mfinlogs::domain
