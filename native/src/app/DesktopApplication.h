#pragma once

#include "app/AppContext.h"

#include <QMainWindow>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>

class QGridLayout;
class QListWidget;
class QStackedWidget;

namespace mfinlogs::app {

class DesktopApplication final : public QMainWindow {
public:
    explicit DesktopApplication(AppContext& context);

private:
    QWidget* buildWelcomePage(QStackedWidget& pages, QListWidget& nav);
    QWidget* buildDailyEntryPage();
    QWidget* buildDashboardPage();
    QWidget* buildPartiesPage();
    QWidget* buildReportPage(const QString& title, const QString& description);
    QWidget* buildInventoryPage();
    QWidget* buildInventoryValuePage();
    QWidget* buildAuditPage();
    QWidget* buildSettingsPage();
    void buildNavigation();
    bool showAuthDialog();
    void loadAuditLogs(QTableWidget& table);
    void loadDashboard(QGridLayout& metricGrid);
    void loadParties(QTableWidget& table);
    void loadInventorySnapshot(QTableWidget& table, const QString& financialYear, int month);
    void loadInventoryValue(QTableWidget& table, const QString& financialYear, int month);
    void loadReportTable(QTableWidget& table, const QString& reportName, const QString& partyName, const domain::ReportRange& range);
    void loadTransactions(QTableWidget& table);
    QJsonArray inventoryRowsFromTable(QTableWidget& table);
    void applyTableSearch(QTableWidget& table, const QString& query);
    QStringList partyNames();
    void setTableRows(QTableWidget& table, const QStringList& headers, const QJsonArray& rows);
    void showError(const QString& title, const std::exception& err);
    void applyTheme();
    void wireActions();

    AppContext& context_;
};

int runDesktopApplication(int argc, char** argv);

} // namespace mfinlogs::app
