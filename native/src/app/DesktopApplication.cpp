#include "app/DesktopApplication.h"

#include "domain/Types.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
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
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
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

QFrame* createContextBar(QWidget* parent) {
    QFrame* bar = new QFrame(parent);
    bar->setObjectName(QStringLiteral("contextBar"));
    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    QLabel* company = new QLabel(QStringLiteral("Company: default"), bar);
    QLabel* year = new QLabel(QStringLiteral("FY: current"), bar);
    QLabel* mode = new QLabel(QStringLiteral("Runtime: native"), bar);
    company->setObjectName(QStringLiteral("contextChip"));
    year->setObjectName(QStringLiteral("contextChip"));
    mode->setObjectName(QStringLiteral("contextChip"));
    layout->addWidget(company);
    layout->addWidget(year);
    layout->addStretch(1);
    layout->addWidget(mode);
    return bar;
}

QWidget* createPageHeader(const QString& title, const QString& subtitle, QWidget* parent) {
    QWidget* header = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel* titleLabel = new QLabel(title, header);
    QLabel* subtitleLabel = new QLabel(subtitle, header);
    titleLabel->setObjectName(QStringLiteral("pageTitle"));
    subtitleLabel->setObjectName(QStringLiteral("pageMeta"));
    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    return header;
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

QListWidgetItem* addGroupItem(QListWidget& nav, const QString& label) {
    QListWidgetItem* item = new QListWidgetItem(label, &nav);
    item->setFlags(Qt::NoItemFlags);
    item->setData(Qt::UserRole, -1);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return item;
}

QListWidgetItem* addNavItem(QListWidget& nav, const QString& label, int pageIndex) {
    QListWidgetItem* item = new QListWidgetItem(label, &nav);
    item->setData(Qt::UserRole, pageIndex);
    return item;
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
    qApp->setFont(QFont(QStringLiteral("Cascadia Mono"), 10));
    qApp->setStyleSheet(QStringLiteral(
        "* { font-family: 'Cascadia Mono', 'Consolas', 'Courier New', monospace; letter-spacing: 0; }"
        "QMainWindow, QWidget#workspace { background: #f4eedf; color: #2d3a33; font-size: 12px; }"
        "QWidget#sidebarWrap { background: #f7efdf; border-right: 1px solid #ddd2bb; }"
        "QListWidget#sidebar { background: transparent; border: 0; color: #536158; padding: 8px 10px 12px 10px; outline: 0; }"
        "QListWidget#sidebar::item { border-radius: 8px; min-height: 32px; padding: 0 10px; margin: 1px 0; }"
        "QListWidget#sidebar::item:selected { background: #e3f0ea; color: #2f7a65; border: 1px solid #b7cec2; }"
        "QListWidget#sidebar::item:hover:!selected { background: #eee5d3; color: #2d3a33; }"
        "QListWidget#sidebar::item:disabled { color: #8a948c; font-size: 11px; font-weight: 700; padding-top: 12px; }"
        "QLabel#brand { color: #2d3a33; font-size: 18px; font-weight: 800; padding: 18px 16px 2px 16px; }"
        "QLabel#brandSub { color: #667268; font-size: 11px; padding: 0 16px 10px 16px; }"
        "QLabel#pageTitle { color: #2d3a33; font-size: 22px; font-weight: 800; }"
        "QLabel#pageMeta { color: #667268; font-size: 11px; }"
        "QLabel#sectionTitle { color: #2d3a33; font-size: 15px; font-weight: 800; }"
        "QLabel#sectionDescription { color: #667268; line-height: 18px; }"
        "QLabel#metricLabel { color: #667268; font-size: 11px; font-weight: 700; }"
        "QLabel#metricValue { color: #2d3a33; font-size: 20px; font-weight: 800; }"
        "QLabel#contextChip { background: #fff9ef; border: 1px solid #cfc1a5; border-radius: 8px; color: #47574f; padding: 5px 9px; }"
        "QFrame#contextBar { background: #fbf5e8; border: 1px solid #ddd2bb; border-radius: 10px; }"
        "QFrame#panel { background: #fbf5e8; border: 1px solid #ddd2bb; border-radius: 10px; }"
        "QTableWidget#dataTable { background: #fbf5e8; alternate-background-color: #f7f1e4; border: 1px solid #ddd2bb; border-radius: 8px; selection-background-color: #e3f0ea; selection-color: #2d3a33; gridline-color: #ddd2bb; }"
        "QHeaderView::section { background: #6b756f; color: #f8f4e8; border: 0; border-right: 1px solid #7b857f; padding: 8px; font-weight: 800; }"
        "QTableWidget::item { padding: 7px; border-bottom: 1px solid #e6dcc8; }"
        "QPushButton { background: #6fae9d; color: #fffaf0; border: 1px solid #4f8f7f; border-radius: 8px; padding: 8px 12px; font-weight: 800; }"
        "QPushButton:hover { background: #4f8f7f; }"
        "QPushButton#secondaryButton { background: #e8f1ed; color: #2f5f4f; border: 1px solid #b7cec2; }"
        "QPushButton#secondaryButton:hover { background: #dcebe4; }"
        "QLineEdit, QDateEdit, QDoubleSpinBox, QComboBox { background: #f7f1e4; border: 1px solid #cfc1a5; border-radius: 8px; min-height: 32px; padding: 0 9px; color: #2d3a33; }"
        "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { background: #fff9ef; border-color: #8ea894; }"
        "QToolBar { background: #fbf5e8; border-bottom: 1px solid #ddd2bb; spacing: 8px; padding: 6px; }"
        "QStatusBar { background: #fbf5e8; color: #667268; border-top: 1px solid #ddd2bb; }"
    ));
}

void DesktopApplication::buildNavigation() {
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("workspace"));
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* sidebar = new QWidget(root);
    sidebar->setObjectName(QStringLiteral("sidebarWrap"));
    sidebar->setFixedWidth(248);
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

    QStackedWidget* pages = new QStackedWidget(root);
    int pageIndex = 0;
    pages->addWidget(buildDailyEntryPage());
    addNavItem(*nav, QStringLiteral("Daily Entry"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildDashboardPage());
    addNavItem(*nav, QStringLiteral("Dashboard"), pageIndex);
    pageIndex += 1;

    addGroupItem(*nav, QStringLiteral("Reports"));
    pages->addWidget(buildReportPage(QStringLiteral("Party Ledger"), QStringLiteral("Party ledger with date filters and running balances.")));
    addNavItem(*nav, QStringLiteral("Party Ledger"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Day Book"), QStringLiteral("Daily transaction register for selected dates.")));
    addNavItem(*nav, QStringLiteral("Day Book"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Daily Summary"), QStringLiteral("Daily sales, returns, receipts, and expenses.")));
    addNavItem(*nav, QStringLiteral("Daily Summary"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Short / Excess"), QStringLiteral("Cash-in-hand snapshots by day.")));
    addNavItem(*nav, QStringLiteral("Short / Excess"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Purchase Report"), QStringLiteral("Purchase summary and supplier analysis.")));
    addNavItem(*nav, QStringLiteral("Purchase Report"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Expenses"), QStringLiteral("Expense transactions and totals.")));
    addNavItem(*nav, QStringLiteral("Expenses"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Outstanding"), QStringLiteral("Customer balances for the selected financial year.")));
    addNavItem(*nav, QStringLiteral("Outstanding"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Trial Balance"), QStringLiteral("Account debit and credit balances.")));
    addNavItem(*nav, QStringLiteral("Trial Balance"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Profit & Loss"), QStringLiteral("Sales, expenses, and net profit summary.")));
    addNavItem(*nav, QStringLiteral("P & L"), pageIndex);
    pageIndex += 1;

    addGroupItem(*nav, QStringLiteral("Inventory"));
    pages->addWidget(buildComingSoonPage(QStringLiteral("Inventory Management"), QStringLiteral("Inventory tables, PDF preview, and email sending are still pending in the native rewrite.")));
    addNavItem(*nav, QStringLiteral("Inventory Management"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildComingSoonPage(QStringLiteral("Stock Value Report"), QStringLiteral("Inventory valuation report will use the native inventory service once implemented.")));
    addNavItem(*nav, QStringLiteral("Stock Value Report"), pageIndex);
    pageIndex += 1;

    addGroupItem(*nav, QStringLiteral("Management"));
    pages->addWidget(buildPartiesPage());
    addNavItem(*nav, QStringLiteral("Add Party"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildAuditPage());
    addNavItem(*nav, QStringLiteral("Audit Logs"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildSettingsPage());
    addNavItem(*nav, QStringLiteral("Settings"), pageIndex);
    sidebarLayout->addWidget(nav, 1);

    connect(nav, &QListWidget::currentRowChanged, this, [nav, pages](int row) {
        const QListWidgetItem* item = nav->item(row);
        if (!item) {
            return;
        }
        const int targetPage = item->data(Qt::UserRole).toInt();
        if (targetPage >= 0) {
            pages->setCurrentIndex(targetPage);
        }
    });
    nav->setCurrentRow(0);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(pages, 1);
    setCentralWidget(root);
}

QWidget* DesktopApplication::buildDailyEntryPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);

    layout->addWidget(createPageHeader(
        QStringLiteral("Daily Transactions"),
        QStringLiteral("Enter transactions quickly and review the current register."),
        page
    ));
    layout->addWidget(createContextBar(page));

    QFrame* entryPanel = createPanel(page);
    QGridLayout* form = new QGridLayout(entryPanel);
    form->setContentsMargins(14, 14, 14, 14);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(10);

    QDateEdit* date = new QDateEdit(QDate::currentDate(), entryPanel);
    date->setCalendarPopup(true);
    QLineEdit* bill = new QLineEdit(entryPanel);
    bill->setPlaceholderText(QStringLiteral("Bill No."));
    QLineEdit* party = new QLineEdit(QStringLiteral("Customer"), entryPanel);
    QComboBox* type = new QComboBox(entryPanel);
    type->addItems({QStringLiteral("Sale"), QStringLiteral("Receipt"), QStringLiteral("Expense"), QStringLiteral("Purchase"), QStringLiteral("Sale Return")});
    QComboBox* mode = new QComboBox(entryPanel);
    mode->addItems({QStringLiteral("Cash"), QStringLiteral("Credit"), QStringLiteral("UPI"), QStringLiteral("Bank")});
    QDoubleSpinBox* amount = new QDoubleSpinBox(entryPanel);
    amount->setMaximum(999999999.0);
    amount->setDecimals(2);
    QPushButton* save = new QPushButton(QStringLiteral("Save Entry"), entryPanel);
    QPushButton* clear = new QPushButton(QStringLiteral("Clear"), entryPanel);
    clear->setObjectName(QStringLiteral("secondaryButton"));

    form->addWidget(new QLabel(QStringLiteral("Date"), entryPanel), 0, 0);
    form->addWidget(date, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("Bill No."), entryPanel), 0, 1);
    form->addWidget(bill, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Party"), entryPanel), 0, 2);
    form->addWidget(party, 1, 2);
    form->addWidget(new QLabel(QStringLiteral("Type"), entryPanel), 0, 3);
    form->addWidget(type, 1, 3);
    form->addWidget(new QLabel(QStringLiteral("Mode"), entryPanel), 0, 4);
    form->addWidget(mode, 1, 4);
    form->addWidget(new QLabel(QStringLiteral("Amount"), entryPanel), 0, 5);
    form->addWidget(amount, 1, 5);
    form->addWidget(save, 1, 6);
    form->addWidget(clear, 1, 7);
    layout->addWidget(entryPanel);

    QFrame* tablePanel = createPanel(page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tablePanel);
    QHBoxLayout* tableHeader = new QHBoxLayout();
    tableHeader->addWidget(createSectionTitle(QStringLiteral("Recent Entries"), tablePanel));
    tableHeader->addStretch(1);
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), tablePanel);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    tableHeader->addWidget(refresh);
    tableLayout->addLayout(tableHeader);
    QTableWidget* table = createDataTable(tablePanel);
    loadTransactions(*table);
    tableLayout->addWidget(table);
    connect(refresh, &QPushButton::clicked, this, [this, table]() { loadTransactions(*table); });
    connect(clear, &QPushButton::clicked, this, [date, bill, party, type, mode, amount]() {
        date->setDate(QDate::currentDate());
        bill->clear();
        party->setText(QStringLiteral("Customer"));
        type->setCurrentIndex(0);
        mode->setCurrentIndex(0);
        amount->setValue(0.0);
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(QStringLiteral("Native save form is visual-only until POST transaction wiring is completed."), 7000);
    });
    layout->addWidget(tablePanel, 1);
    return page;
}

QWidget* DesktopApplication::buildDashboardPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);

    layout->addWidget(createPageHeader(
        QStringLiteral("Dashboard"),
        QStringLiteral("Live accounting overview from the configured SQL Server database."),
        page
    ));
    layout->addWidget(createContextBar(page));

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

QWidget* DesktopApplication::buildPartiesPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Add Party"), QStringLiteral("Create and review customer, supplier, bank, and expense parties."), page));
    layout->addWidget(createContextBar(page));

    QFrame* formPanel = createPanel(page);
    QGridLayout* form = new QGridLayout(formPanel);
    QLineEdit* name = new QLineEdit(formPanel);
    name->setPlaceholderText(QStringLiteral("Enter name..."));
    QComboBox* type = new QComboBox(formPanel);
    type->addItems({QStringLiteral("Customer"), QStringLiteral("Credit Customer"), QStringLiteral("Supplier"), QStringLiteral("Bank"), QStringLiteral("Expense")});
    QPushButton* create = new QPushButton(QStringLiteral("Create Party"), formPanel);
    form->addWidget(new QLabel(QStringLiteral("Party Name"), formPanel), 0, 0);
    form->addWidget(name, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("Type"), formPanel), 0, 1);
    form->addWidget(type, 1, 1);
    form->addWidget(create, 1, 2);
    connect(create, &QPushButton::clicked, this, [this]() {
        statusBar()->showMessage(QStringLiteral("Native create-party form is visual-only until POST party wiring is completed."), 7000);
    });
    layout->addWidget(formPanel);

    QTableWidget* table = createDataTable(page);
    loadParties(*table);
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildReportPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);

    layout->addWidget(createPageHeader(title, description, page));
    layout->addWidget(createContextBar(page));

    QFrame* filters = createPanel(page);
    QHBoxLayout* filterLayout = new QHBoxLayout(filters);
    QLineEdit* search = new QLineEdit(filters);
    search->setPlaceholderText(QStringLiteral("Search party, bill, amount..."));
    QDateEdit* start = new QDateEdit(QDate::currentDate().addDays(-30), filters);
    start->setCalendarPopup(true);
    QDateEdit* end = new QDateEdit(QDate::currentDate(), filters);
    end->setCalendarPopup(true);
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), filters);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    filterLayout->addWidget(search, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("Start"), filters));
    filterLayout->addWidget(start);
    filterLayout->addWidget(new QLabel(QStringLiteral("End"), filters));
    filterLayout->addWidget(end);
    filterLayout->addWidget(refresh);
    layout->addWidget(filters);

    QTableWidget* table = createDataTable(page);
    loadReportTable(*table, title);
    connect(refresh, &QPushButton::clicked, this, [this, table, title]() { loadReportTable(*table, title); });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildAuditPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Audit Logs"), QStringLiteral("Administrative activity and native service events."), page));
    layout->addWidget(createContextBar(page));

    QTableWidget* table = createDataTable(page);
    loadAuditLogs(*table);
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildSettingsPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Settings"), QStringLiteral("Database configuration, backup paths, and native runtime controls."), page));
    layout->addWidget(createContextBar(page));

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
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(title, description, page));
    layout->addWidget(createContextBar(page));
    QFrame* panel = createPanel(page);
    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->addWidget(createSectionTitle(QStringLiteral("Native parity pending"), panel));
    panelLayout->addWidget(createSectionDescription(description, panel));
    layout->addWidget(panel);
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
        if (reportName == QStringLiteral("Party Ledger")) {
            QJsonArray rows;
            QJsonObject placeholder;
            placeholder.insert(QStringLiteral("date"), QStringLiteral("--"));
            placeholder.insert(QStringLiteral("bill_no"), QStringLiteral("--"));
            placeholder.insert(QStringLiteral("type"), QStringLiteral("Select party in native service wiring"));
            placeholder.insert(QStringLiteral("mode"), QStringLiteral("--"));
            placeholder.insert(QStringLiteral("amount"), 0.0);
            placeholder.insert(QStringLiteral("balance"), 0.0);
            rows.append(placeholder);
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount"), QStringLiteral("balance")}, rows);
            return;
        }
        if (reportName == QStringLiteral("Day Book")) {
            const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 100, 1);
            QJsonArray data;
            for (const domain::TransactionRow& row : rows) {
                data.append(transactionToJson(row));
            }
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")}, data);
            return;
        }
        if (reportName == QStringLiteral("Purchase Report") || reportName == QStringLiteral("Expenses")) {
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")}, QJsonArray());
            return;
        }
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
