#include "app/DesktopApplication.h"

#include "domain/Types.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QToolBar>
#include <QVariant>
#include <QVBoxLayout>

#include <exception>

namespace {

QLabel* createSectionTitle(const QString& title, QWidget* parent) {
    QLabel* label = new QLabel(title, parent);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

QLabel* createSectionDescription(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionDescription"));
    label->setWordWrap(true);
    return label;
}

QFrame* createPanel(QWidget* parent) {
    QFrame* panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("panel"));
    panel->setFrameShape(QFrame::NoFrame);
    return panel;
}

QTableWidget* createDataTable(QWidget* parent) {
    QTableWidget* table = new QTableWidget(parent);
    table->setObjectName(QStringLiteral("dataTable"));
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setShowGrid(false);
    return table;
}

QFrame* createMetricTile(const QString& label, const QString& value, QWidget* parent) {
    QFrame* tile = createPanel(parent);
    QVBoxLayout* layout = new QVBoxLayout(tile);
    QLabel* titleLabel = new QLabel(label, tile);
    QLabel* valueLabel = new QLabel(value, tile);
    titleLabel->setObjectName(QStringLiteral("metricLabel"));
    valueLabel->setObjectName(QStringLiteral("metricValue"));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return tile;
}

QString moneyText(double value) {
    return QStringLiteral("%1").arg(value, 0, 'f', 2);
}

QJsonObject transactionToJson(const mfinlogs::domain::TransactionRow& row) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), row.id);
    item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
    item.insert(QStringLiteral("bill_no"), row.billNo);
    item.insert(QStringLiteral("party"), row.party);
    item.insert(QStringLiteral("amount"), row.amount);

    switch (row.type) {
    case mfinlogs::domain::TransactionType::Sale:
        item.insert(QStringLiteral("type"), QStringLiteral("Sale"));
        break;
    case mfinlogs::domain::TransactionType::SaleReturn:
        item.insert(QStringLiteral("type"), QStringLiteral("Sale Return"));
        break;
    case mfinlogs::domain::TransactionType::Purchase:
        item.insert(QStringLiteral("type"), QStringLiteral("Purchase"));
        break;
    case mfinlogs::domain::TransactionType::Expense:
        item.insert(QStringLiteral("type"), QStringLiteral("Expense"));
        break;
    case mfinlogs::domain::TransactionType::Receipt:
        item.insert(QStringLiteral("type"), QStringLiteral("Receipt"));
        break;
    }

    switch (row.mode) {
    case mfinlogs::domain::PaymentMode::Credit:
        item.insert(QStringLiteral("mode"), QStringLiteral("Credit"));
        break;
    case mfinlogs::domain::PaymentMode::Cash:
        item.insert(QStringLiteral("mode"), QStringLiteral("Cash"));
        break;
    case mfinlogs::domain::PaymentMode::Upi:
        item.insert(QStringLiteral("mode"), QStringLiteral("UPI"));
        break;
    case mfinlogs::domain::PaymentMode::Bank:
        item.insert(QStringLiteral("mode"), QStringLiteral("Bank"));
        break;
    }

    return item;
}

} // namespace

namespace mfinlogs::app {

DesktopApplication::DesktopApplication(AppContext& context)
    : context_(context) {
    setWindowTitle(QStringLiteral("M-Finlogs Native"));
    resize(1360, 860);
    applyTheme();
    buildNavigation();
    wireActions();
}

void DesktopApplication::applyTheme() {
    qApp->setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget#workspace { background: #f5f7fb; color: #182230; font-family: 'Segoe UI'; font-size: 13px; }"
        "QListWidget#sidebar { background: #111827; border: 0; color: #cbd5e1; padding: 10px 8px; }"
        "QListWidget#sidebar::item { border-radius: 6px; min-height: 34px; padding: 0 12px; }"
        "QListWidget#sidebar::item:selected { background: #2563eb; color: #ffffff; }"
        "QListWidget#sidebar::item:hover:!selected { background: #1f2937; color: #ffffff; }"
        "QLabel#brand { color: #ffffff; font-size: 18px; font-weight: 700; padding: 18px 18px 4px 18px; }"
        "QLabel#brandSub { color: #94a3b8; font-size: 12px; padding: 0 18px 14px 18px; }"
        "QLabel#pageTitle { color: #101828; font-size: 24px; font-weight: 700; }"
        "QLabel#pageMeta { color: #64748b; font-size: 12px; }"
        "QLabel#sectionTitle { color: #101828; font-size: 16px; font-weight: 700; }"
        "QLabel#sectionDescription { color: #64748b; line-height: 18px; }"
        "QLabel#metricLabel { color: #667085; font-size: 12px; }"
        "QLabel#metricValue { color: #101828; font-size: 22px; font-weight: 700; }"
        "QFrame#panel { background: #ffffff; border: 1px solid #e5e7eb; border-radius: 8px; }"
        "QTableWidget#dataTable { background: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e5e7eb; border-radius: 8px; selection-background-color: #dbeafe; selection-color: #111827; }"
        "QHeaderView::section { background: #f1f5f9; color: #334155; border: 0; border-bottom: 1px solid #e5e7eb; padding: 8px; font-weight: 600; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #eef2f7; }"
        "QPushButton { background: #2563eb; color: #ffffff; border: 0; border-radius: 6px; padding: 8px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #1d4ed8; }"
        "QPushButton#secondaryButton { background: #ffffff; color: #334155; border: 1px solid #cbd5e1; }"
        "QPushButton#secondaryButton:hover { background: #f8fafc; }"
        "QLineEdit { background: #ffffff; border: 1px solid #cbd5e1; border-radius: 6px; min-height: 34px; padding: 0 10px; }"
        "QToolBar { background: #ffffff; border-bottom: 1px solid #e5e7eb; spacing: 8px; padding: 6px; }"
    ));
}

void DesktopApplication::buildNavigation() {
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("workspace"));
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* sidebar = new QWidget(root);
    sidebar->setFixedWidth(248);
    sidebar->setStyleSheet(QStringLiteral("background: #111827;"));
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    QLabel* brand = new QLabel(QStringLiteral("M-Finlogs"), sidebar);
    QLabel* brandSub = new QLabel(QStringLiteral("Native accounting workspace"), sidebar);
    brand->setObjectName(QStringLiteral("brand"));
    brandSub->setObjectName(QStringLiteral("brandSub"));
    sidebarLayout->addWidget(brand);
    sidebarLayout->addWidget(brandSub);

    QListWidget* nav = new QListWidget(sidebar);
    nav->setObjectName(QStringLiteral("sidebar"));
    nav->addItems({
        QStringLiteral("Dashboard"),
        QStringLiteral("Transactions"),
        QStringLiteral("Parties"),
        QStringLiteral("Outstanding"),
        QStringLiteral("Trial Balance"),
        QStringLiteral("Profit & Loss"),
        QStringLiteral("Daily Summary"),
        QStringLiteral("Short / Excess"),
        QStringLiteral("Inventory"),
        QStringLiteral("Audit Logs"),
        QStringLiteral("Settings")
    });
    sidebarLayout->addWidget(nav, 1);

    QStackedWidget* pages = new QStackedWidget(root);
    pages->addWidget(buildDashboardPage());
    pages->addWidget(buildTransactionsPage());
    pages->addWidget(buildPartiesPage());
    pages->addWidget(buildReportPage(QStringLiteral("Outstanding"), QStringLiteral("Customer balances for the selected financial year.")));
    pages->addWidget(buildReportPage(QStringLiteral("Trial Balance"), QStringLiteral("Account debit and credit balances.")));
    pages->addWidget(buildReportPage(QStringLiteral("Profit & Loss"), QStringLiteral("Sales, expenses, and net profit summary.")));
    pages->addWidget(buildReportPage(QStringLiteral("Daily Summary"), QStringLiteral("Daily sales, returns, receipts, and expenses.")));
    pages->addWidget(buildReportPage(QStringLiteral("Short / Excess"), QStringLiteral("Cash-in-hand snapshots by day.")));
    pages->addWidget(buildComingSoonPage(QStringLiteral("Inventory"), QStringLiteral("Inventory tables, PDF preview, and email sending are still pending in the native rewrite.")));
    pages->addWidget(buildAuditPage());
    pages->addWidget(buildSettingsPage());

    connect(nav, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(pages, 1);
    setCentralWidget(root);
}

QWidget* DesktopApplication::buildDashboardPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(18);

    QLabel* title = new QLabel(QStringLiteral("Dashboard"), page);
    QLabel* meta = new QLabel(QStringLiteral("Live accounting overview from the configured SQL Server database."), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    meta->setObjectName(QStringLiteral("pageMeta"));
    layout->addWidget(title);
    layout->addWidget(meta);

    QGridLayout* metrics = new QGridLayout();
    metrics->setSpacing(12);
    loadDashboard(*metrics);
    layout->addLayout(metrics);

    QFrame* panel = createPanel(page);
    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->addWidget(createSectionTitle(QStringLiteral("Recent Transactions"), panel));
    QTableWidget* table = createDataTable(panel);
    loadTransactions(*table);
    panelLayout->addWidget(table);
    layout->addWidget(panel, 1);
    return page;
}

QWidget* DesktopApplication::buildTransactionsPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    QLabel* title = new QLabel(QStringLiteral("Transactions"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    QFrame* actions = createPanel(page);
    QHBoxLayout* actionLayout = new QHBoxLayout(actions);
    QLineEdit* search = new QLineEdit(actions);
    search->setPlaceholderText(QStringLiteral("Search party, bill, type"));
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), actions);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    actionLayout->addWidget(search, 1);
    actionLayout->addWidget(refresh);
    layout->addWidget(actions);

    QTableWidget* table = createDataTable(page);
    loadTransactions(*table);
    connect(refresh, &QPushButton::clicked, this, [this, table]() { loadTransactions(*table); });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildPartiesPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    QLabel* title = new QLabel(QStringLiteral("Parties"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    QTableWidget* table = createDataTable(page);
    loadParties(*table);
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildReportPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    QLabel* heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);
    layout->addWidget(createSectionDescription(description, page));

    QTableWidget* table = createDataTable(page);
    loadReportTable(*table, title);
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildAuditPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    QLabel* title = new QLabel(QStringLiteral("Audit Logs"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    QTableWidget* table = createDataTable(page);
    loadAuditLogs(*table);
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildSettingsPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);

    QLabel* title = new QLabel(QStringLiteral("Settings"), page);
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    QFrame* panel = createPanel(page);
    QGridLayout* form = new QGridLayout(panel);
    const domain::DatabaseConfig config = context_.services().config->readDatabaseConfig();
    form->addWidget(new QLabel(QStringLiteral("Server"), panel), 0, 0);
    form->addWidget(new QLabel(config.server, panel), 0, 1);
    form->addWidget(new QLabel(QStringLiteral("Database"), panel), 1, 0);
    form->addWidget(new QLabel(config.database, panel), 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Authentication"), panel), 2, 0);
    form->addWidget(new QLabel(config.useWindowsAuth ? QStringLiteral("Windows") : QStringLiteral("SQL"), panel), 2, 1);
    form->addWidget(new QLabel(QStringLiteral("Backup Directory"), panel), 3, 0);
    form->addWidget(new QLabel(config.backupDir, panel), 3, 1);
    layout->addWidget(panel);
    layout->addStretch(1);
    return page;
}

QWidget* DesktopApplication::buildComingSoonPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(14);
    QLabel* heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);
    layout->addWidget(createSectionDescription(description, page));
    layout->addStretch(1);
    return page;
}

void DesktopApplication::loadDashboard(QGridLayout& metricGrid) {
    try {
        const QJsonObject metrics = context_.services().reports->dashboardMetrics();
        metricGrid.addWidget(createMetricTile(QStringLiteral("Sales Today"), moneyText(metrics.value(QStringLiteral("sales_today")).toDouble()), this), 0, 0);
        metricGrid.addWidget(createMetricTile(QStringLiteral("Sales This Month"), moneyText(metrics.value(QStringLiteral("sales_month")).toDouble()), this), 0, 1);
        metricGrid.addWidget(createMetricTile(QStringLiteral("Cash Balance"), moneyText(metrics.value(QStringLiteral("cash_balance")).toDouble()), this), 0, 2);
        metricGrid.addWidget(createMetricTile(QStringLiteral("Bank Balance"), moneyText(metrics.value(QStringLiteral("bank_balance")).toDouble()), this), 0, 3);
        metricGrid.addWidget(createMetricTile(QStringLiteral("Receivables"), moneyText(metrics.value(QStringLiteral("receivables")).toDouble()), this), 0, 4);
    } catch (const std::exception& err) {
        metricGrid.addWidget(createMetricTile(QStringLiteral("Database"), QStringLiteral("Unavailable"), this), 0, 0);
        showError(QStringLiteral("Dashboard"), err);
    }
}

void DesktopApplication::loadTransactions(QTableWidget& table) {
    try {
        const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 100, 365);
        QJsonArray data;
        for (const domain::TransactionRow& row : rows) {
            data.append(transactionToJson(row));
        }
        setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")}, data);
    } catch (const std::exception& err) {
        showError(QStringLiteral("Transactions"), err);
    }
}

void DesktopApplication::loadParties(QTableWidget& table) {
    try {
        setTableRows(table, {QStringLiteral("name"), QStringLiteral("type")}, context_.services().parties->listParties());
    } catch (const std::exception& err) {
        showError(QStringLiteral("Parties"), err);
    }
}

void DesktopApplication::loadAuditLogs(QTableWidget& table) {
    try {
        setTableRows(table, {QStringLiteral("timestamp"), QStringLiteral("username"), QStringLiteral("action"), QStringLiteral("details")}, context_.services().audit->listAuditLogs(QStringLiteral("native")));
    } catch (const std::exception& err) {
        showError(QStringLiteral("Audit Logs"), err);
    }
}

void DesktopApplication::loadReportTable(QTableWidget& table, const QString& reportName) {
    try {
        if (reportName == QStringLiteral("Outstanding")) {
            const QJsonObject report = context_.services().reports->outstanding();
            setTableRows(table, {QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("balance")}, report.value(QStringLiteral("data")).toArray());
            return;
        }
        if (reportName == QStringLiteral("Trial Balance")) {
            setTableRows(table, {QStringLiteral("account"), QStringLiteral("debit"), QStringLiteral("credit"), QStringLiteral("balance")}, context_.services().reports->trialBalance());
            return;
        }
        if (reportName == QStringLiteral("Profit & Loss")) {
            const QJsonObject report = context_.services().reports->profitAndLoss();
            QJsonArray rows;
            QJsonObject sales;
            sales.insert(QStringLiteral("metric"), QStringLiteral("Sales"));
            sales.insert(QStringLiteral("amount"), report.value(QStringLiteral("sales")).toDouble());
            rows.append(sales);
            QJsonObject expenses;
            expenses.insert(QStringLiteral("metric"), QStringLiteral("Expenses"));
            expenses.insert(QStringLiteral("amount"), report.value(QStringLiteral("expenses")).toDouble());
            rows.append(expenses);
            QJsonObject netProfit;
            netProfit.insert(QStringLiteral("metric"), QStringLiteral("Net Profit"));
            netProfit.insert(QStringLiteral("amount"), report.value(QStringLiteral("net_profit")).toDouble());
            rows.append(netProfit);
            setTableRows(table, {QStringLiteral("metric"), QStringLiteral("amount")}, rows);
            return;
        }

        const domain::ReportRange range{QDate::currentDate().addDays(-30), QDate::currentDate(), 30};
        if (reportName == QStringLiteral("Daily Summary")) {
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("sales"), QStringLiteral("sale_returns"), QStringLiteral("receipts"), QStringLiteral("expenses"), QStringLiteral("net_sales")}, context_.services().reports->dailySummary(range));
            return;
        }
        if (reportName == QStringLiteral("Short / Excess")) {
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("cash_in_hand")}, context_.services().reports->shortExcess(range));
        }
    } catch (const std::exception& err) {
        showError(reportName, err);
    }
}

void DesktopApplication::setTableRows(QTableWidget& table, const QStringList& headers, const QJsonArray& rows) {
    table.clear();
    table.setColumnCount(headers.size());
    table.setRowCount(rows.size());
    table.setHorizontalHeaderLabels(headers);

    for (int rowIndex = 0; rowIndex < rows.size(); rowIndex += 1) {
        const QJsonObject row = rows.at(rowIndex).toObject();
        for (int columnIndex = 0; columnIndex < headers.size(); columnIndex += 1) {
            const QString key = headers.at(columnIndex);
            const QJsonValue value = row.value(key);
            QString text;
            if (value.isDouble()) {
                text = moneyText(value.toDouble());
            } else {
                text = value.toVariant().toString();
            }
            table.setItem(rowIndex, columnIndex, new QTableWidgetItem(text));
        }
    }
    table.resizeColumnsToContents();
}

void DesktopApplication::showError(const QString& title, const std::exception& err) {
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, QString::fromUtf8(err.what())), 8000);
}

void DesktopApplication::wireActions() {
    QToolBar* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    QAction* updateAction = toolbar->addAction(QStringLiteral("Check Updates"));
    QAction* backupAction = toolbar->addAction(QStringLiteral("Backup"));

    connect(updateAction, &QAction::triggered, this, [this]() {
        try {
            context_.services().updates->checkForUpdates();
        } catch (const std::exception& err) {
            showError(QStringLiteral("Updates"), err);
        }
    });

    connect(backupAction, &QAction::triggered, this, [this]() {
        try {
            const domain::BackupResult result = context_.services().backups->autoBackup();
            QMessageBox::information(this, QStringLiteral("Backup"), result.status + QStringLiteral("\n") + result.path);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Backup"), err);
        }
    });
}

int runDesktopApplication(int argc, char** argv) {
    QApplication qtApp(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("M-Finlogs"));
    QApplication::setApplicationName(QStringLiteral("M-Finlogs"));

    AppContext context(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
    DesktopApplication window(context);
    window.show();
    return qtApp.exec();
}

} // namespace mfinlogs::app
