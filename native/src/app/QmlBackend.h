#pragma once

#include "app/AppContext.h"

#include <QAtomicInt>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace mfinlogs::app {

// QmlBackend is the single bridge object exposed to the QML "Aurora" UI.
//
// It wraps the existing domain::ServiceRegistry (auth, transactions, parties,
// reports, audit, ...) and converts QJson* payloads into QVariant structures
// that QML can bind to directly. Every call that can throw is wrapped so the
// UI receives a structured { ok, error, ... } result instead of crashing.
//
// Threading model:
// - All fetchX() methods submit work to QThreadPool::globalInstance()
// - Each request carries an immutable generation counter
// - Stale results (generation mismatch) are silently discarded
// - Results are delivered via Qt::QueuedConnection back to the GUI thread
// - Page destruction safety: QPointer guards the receiver
// - Loading state is tracked and exposed to QML via loadingStates property
class QmlBackend final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY authChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY authChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY authChanged)
    Q_PROPERTY(QString companyName READ companyName NOTIFY authChanged)
    Q_PROPERTY(bool setupRequired READ setupRequired NOTIFY setupChanged)
    Q_PROPERTY(QVariantMap loadingStates READ loadingStates NOTIFY loadingStatesChanged)

public:
    explicit QmlBackend(AppContext& context, QObject* parent = nullptr);
    ~QmlBackend() override;

    bool authenticated() const { return authenticated_; }
    QString currentUser() const { return currentUser_; }
    QString currentRole() const { return currentRole_; }
    bool isAdmin() const { return currentRole_ == QStringLiteral("admin"); }
    QString companyName() const { return companyName_; }
    bool setupRequired() const;
    QVariantMap loadingStates() const;

    // --- Auth & session ---------------------------------------------------
    Q_INVOKABLE QVariantMap login(const QString& username, const QString& password);
    Q_INVOKABLE QVariantMap setupAdmin(const QString& username, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE QVariantList companies();

    // --- Dashboard --------------------------------------------------------
    Q_INVOKABLE QVariantMap dashboard();
    Q_INVOKABLE QVariantList salesTrend(int days);

    // --- Transactions / Daily entry --------------------------------------
    Q_INVOKABLE QVariantList transactions(int page, int limit, int days);
    Q_INVOKABLE QVariantMap addTransaction(const QString& dateIso,
                                           const QString& billNo,
                                           const QString& party,
                                           const QString& type,
                                           const QString& mode,
                                           double amount);
    Q_INVOKABLE QVariantMap editTransaction(int id, const QString& dateIso, const QString& billNo, const QString& party, const QString& type, const QString& mode, double amount);
    Q_INVOKABLE QVariantMap deleteTransaction(int id);
    Q_INVOKABLE QString nextBillNumber(const QString& billNo);

    // --- Parties ----------------------------------------------------------
    Q_INVOKABLE QVariantList parties();
    Q_INVOKABLE QStringList partyNames();
    Q_INVOKABLE QVariantMap createParty(const QString& name, const QString& type, bool creditAllowed);
    Q_INVOKABLE QVariantMap renameParty(const QString& oldName, const QString& newName);

    // --- Reports ----------------------------------------------------------
    Q_INVOKABLE QVariantMap ledger(const QString& party, const QString& startIso, const QString& endIso);
    Q_INVOKABLE QVariantList dayBook(const QString& dateIso);
    Q_INVOKABLE QVariantList dailySummary(const QString& startIso, const QString& endIso);
    Q_INVOKABLE QVariantMap outstanding();
    Q_INVOKABLE QVariantList trialBalance();
    Q_INVOKABLE QVariantMap profitAndLoss();
    Q_INVOKABLE QVariantMap saveCashInHand(const QString& dateIso, double amount);
    Q_INVOKABLE double openingCashSeed();
    Q_INVOKABLE QVariantMap saveOpeningCashSeed(double amount);

    // --- Inventory --------------------------------------------------------
    Q_INVOKABLE QVariantList financialYears();
    Q_INVOKABLE QVariantList inventorySnapshot(const QString& financialYear, int month);
    Q_INVOKABLE QVariantMap saveInventory(const QString& financialYear, int month, const QVariantList& rows);
    Q_INVOKABLE QVariantList stockValue(const QString& financialYear, int month);
    Q_INVOKABLE QVariantMap inventoryPdfPreview(const QString& financialYear, int month, bool onlyReorder);

    // --- Users (admin) ----------------------------------------------------
    Q_INVOKABLE QVariantList users();
    Q_INVOKABLE QVariantMap createUser(const QString& username, const QString& password, const QString& role);
    Q_INVOKABLE QVariantMap changePassword(const QString& username, const QString& newPassword);
    Q_INVOKABLE QVariantMap deleteUser(const QString& username);

    // --- Database config ---------------------------------------------------
    Q_INVOKABLE QVariantMap readDatabaseConfig();
    Q_INVOKABLE QVariantMap testDatabaseConfig(const QVariantMap& config);
    Q_INVOKABLE QVariantMap saveDatabaseConfig(const QVariantMap& config);

    // --- Backup & restore -------------------------------------------------
    Q_INVOKABLE QVariantMap backupDatabase(const QString& targetDir);
    Q_INVOKABLE QVariantMap autoBackup();
    Q_INVOKABLE QVariantMap restoreDatabase(const QString& backupPath);

    // --- Server controls --------------------------------------------------
    Q_INVOKABLE QVariantMap startNativeServer();
    Q_INVOKABLE QVariantMap stopNativeServer();

    // --- Audit ------------------------------------------------------------
    Q_INVOKABLE QVariantList auditLogs();

    // --- Import / Export ---------------------------------------------------
    Q_INVOKABLE QVariantMap importTransactions();
    Q_INVOKABLE QVariantMap smartImportExcel();
    Q_INVOKABLE QVariantMap exportTableToPdf(const QString& title, const QVariantList& columns, const QVariantList& rows);
    Q_INVOKABLE QVariantMap exportTableToExcel(const QString& title, const QVariantList& columns, const QVariantList& rows);
    Q_INVOKABLE QVariantMap exportRecentPdf(int days);
    Q_INVOKABLE QVariantMap exportRecentExcel(int days);
    Q_INVOKABLE QVariantMap downloadImportTemplate();

    // --- Batch operations & undo ------------------------------------------
    Q_INVOKABLE QVariantMap batchDeleteTransactions(const QVariantList& ids);
    Q_INVOKABLE QVariantMap undoDeleteTransaction();

    // --- Party balance lookup (for entry hints) ---------------------------
    Q_INVOKABLE QVariantMap partyBalance(const QString& partyName);
    Q_INVOKABLE QVariantList allPartyBalances();
    Q_INVOKABLE QVariantList creditFollowupList();
    Q_INVOKABLE QVariantMap closingReport();

    // --- Mode selection (Server / Local / Migration) ----------------------
    Q_INVOKABLE QString currentMode() const;
    Q_INVOKABLE QVariantMap selectMode(const QString& mode);
    Q_INVOKABLE QVariantMap migrateServerToLocal();

    // --- Formatting helpers (used widely by QML) --------------------------
    Q_INVOKABLE QString formatMoney(double value) const;
    Q_INVOKABLE QString formatDate(const QString& iso) const;
    Q_INVOKABLE QString todayIso() const;

    // --- Asynchronous Reports Loading ------------------------------------
    // Each fetchX() increments a per-type generation counter for stale
    // request cancellation. If the user changes filters rapidly, only the
    // most recent request's result is delivered.
    Q_INVOKABLE void fetchLedger(const QString& party, const QString& startIso, const QString& endIso);
    Q_INVOKABLE void fetchDayBook(const QString& dateIso);
    Q_INVOKABLE void fetchDashboard();
    Q_INVOKABLE void fetchDailySummary(const QString& startIso, const QString& endIso);
    Q_INVOKABLE void fetchOutstanding();
    Q_INVOKABLE void fetchTrialBalance();
    Q_INVOKABLE void fetchProfitAndLoss();
    Q_INVOKABLE void fetchPartyBalances();
    Q_INVOKABLE void fetchAuditLogs();
    Q_INVOKABLE void fetchInventoryReport(const QString& financialYear, int month);
    Q_INVOKABLE void fetchTransactions(int page, int limit, int days);

    // --- Async Export Operations ------------------------------------------
    // Runs PDF/Excel generation in a background thread to avoid blocking GUI.
    Q_INVOKABLE void fetchExport(const QString& format, const QString& title,
                                 const QVariantList& columns, const QVariantList& rows);

    // --- Async Backup & Restore -------------------------------------------
    Q_INVOKABLE void fetchBackup(const QString& targetDir);
    Q_INVOKABLE void fetchRestore(const QString& backupPath);

signals:
    void authChanged();
    void setupChanged();
    void dataChanged();
    void toast(const QString& message, const QString& kind);
    void loadingStatesChanged();

    // --- Async report result signals --------------------------------------
    void ledgerLoaded(const QVariantMap& result);
    void dayBookLoaded(const QVariantList& result);
    void dashboardLoaded(const QVariantMap& result);
    void dailySummaryLoaded(const QVariantList& result);
    void outstandingLoaded(const QVariantMap& result);
    void trialBalanceLoaded(const QVariantList& result);
    void profitAndLossLoaded(const QVariantMap& result);
    void partyBalancesLoaded(const QVariantList& result);
    void auditLogsLoaded(const QVariantList& result);
    void inventoryReportLoaded(const QVariantMap& result);
    void transactionsLoaded(const QVariantList& result);

    // --- Async export/backup result signals -------------------------------
    void exportComplete(const QVariantMap& result);
    void backupComplete(const QVariantMap& result);
    void restoreComplete(const QVariantMap& result);

    // --- Timing diagnostics signal ----------------------------------------
    void reportTimingComplete(const QString& reportName, const QVariantMap& timings);

public slots:
    void onLedgerTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onDayBookTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onDashboardTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onDailySummaryTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onOutstandingTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onTrialBalanceTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onProfitAndLossTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onPartyBalancesTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onAuditLogsTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onInventoryReportTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onTransactionsTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings);
    void onExportTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onBackupTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);
    void onRestoreTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings);

private:
    AppContext& context_;
    bool authenticated_ = false;
    QString currentUser_;
    QString currentRole_;
    QString companyName_;

    struct UndoBuffer {
        int transactionId = -1;
        domain::TransactionCreateRequest request;
    };
    UndoBuffer undoBuffer_;

    // --- Request tracking for stale cancellation and duplicate prevention --
    // Each TaskType has its own generation counter. When a new request is
    // submitted, the counter increments. When the result arrives, it is
    // discarded if the generation does not match the current value.
    struct RequestTracker {
        QAtomicInt generation{0};
    };

    // Map from task type enum int to its tracker.
    // Accessed only from the GUI thread (fetchX and onXTaskFinished) so
    // no mutex is needed for the map itself; the QAtomicInt inside handles
    // the cross-thread read in DatabaseTask::run().
    QHash<int, RequestTracker> requestTrackers_;

    // Loading state per task type (true = in-flight request exists)
    QHash<int, bool> loadingFlags_;

    // Helpers
    int nextGeneration(int taskType);
    bool isStale(int taskType, int generation) const;
    void setLoading(int taskType, bool loading);
};

} // namespace mfinlogs::app
