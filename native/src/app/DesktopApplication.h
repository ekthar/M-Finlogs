#pragma once

#include "app/AppContext.h"

#include <QMainWindow>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>

class QGridLayout;

namespace mfinlogs::app {

class DesktopApplication final : public QMainWindow {
public:
    explicit DesktopApplication(AppContext& context);

private:
    QWidget* buildDailyEntryPage();
    QWidget* buildDashboardPage();
    QWidget* buildPartiesPage();
    QWidget* buildReportPage(const QString& title, const QString& description);
    QWidget* buildAuditPage();
    QWidget* buildSettingsPage();
    QWidget* buildComingSoonPage(const QString& title, const QString& description);
    void buildNavigation();
    void loadAuditLogs(QTableWidget& table);
    void loadDashboard(QGridLayout& metricGrid);
    void loadParties(QTableWidget& table);
    void loadReportTable(QTableWidget& table, const QString& reportName, const QString& partyName, const domain::ReportRange& range);
    void loadTransactions(QTableWidget& table);
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
