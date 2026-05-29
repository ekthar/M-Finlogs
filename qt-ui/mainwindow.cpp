#include "mainwindow.h"
#include "style.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QDate>
#include <QSplitter>
#include <QScrollArea>
#include <QSpacerItem>

// ============================================================================
// M-Finlogs Native — MainWindow Implementation
// Premium Mac-grade minimalist UI with Qt Widgets
// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Apply global stylesheet
    qApp->setStyleSheet(MFinlogs::globalStyleSheet());

    setWindowTitle("M-Finlogs Native");
    setMinimumSize(1100, 700);
    resize(1280, 780);

    setupUi();
}

// ----------------------------------------------------------------------------
// Top-level layout: Sidebar | Content
// ----------------------------------------------------------------------------
void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Sidebar ---
    setupSidebar();
    mainLayout->addWidget(m_sidebarNav);

    // --- Content Area ---
    QWidget *contentWidget = new QWidget;
    contentWidget->setStyleSheet("background-color: #FFFFFF;");
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(28, 24, 28, 24);
    contentLayout->setSpacing(20);

    setupContentArea();

    // Page Title + Subtitle
    QVBoxLayout *headerLayout = new QVBoxLayout;
    headerLayout->setSpacing(4);
    headerLayout->addWidget(m_pageTitle);
    headerLayout->addWidget(m_pageSubtitle);
    contentLayout->addLayout(headerLayout);

    // Input Form
    setupInputForm();
    contentLayout->addWidget(m_inputFormContainer);

    // Recent Entries Label + Search + Table
    QHBoxLayout *tableHeaderLayout = new QHBoxLayout;
    QLabel *recentLabel = new QLabel("Recent Entries");
    recentLabel->setStyleSheet("font-size: 11pt; font-weight: 600; color: #111827;");
    tableHeaderLayout->addWidget(recentLabel);
    tableHeaderLayout->addStretch();
    tableHeaderLayout->addWidget(m_searchBar);
    contentLayout->addLayout(tableHeaderLayout);

    setupTable();
    contentLayout->addWidget(m_table, 1); // stretch factor = 1 so table takes remaining space

    mainLayout->addWidget(contentWidget, 1);
}

// ----------------------------------------------------------------------------
// Sidebar Navigation
// ----------------------------------------------------------------------------
void MainWindow::setupSidebar()
{
    m_sidebarNav = new QListWidget;
    m_sidebarNav->setObjectName("sidebarNav");
    m_sidebarNav->setFixedWidth(190);
    m_sidebarNav->setFrameShape(QFrame::NoFrame);
    m_sidebarNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Disable the dotted focus rectangle on items
    m_sidebarNav->setFocusPolicy(Qt::NoFocus);

    // Navigation items (matching your screenshot)
    QStringList navItems = {
        "Welcome",
        "Daily Entry",
        "Dashboard",
        "Reports",
        "Party Ledger",
        "Day Book",
        "Daily Summary",
        "Short / Excess",
        "Expenses",
        "Outstanding",
        "P & L",
        "Inventory",
        "Inventory Management",
        "Stock Value Report",
        "Management",
        "Add Party",
        "Audit Logs",
        "Settings"
    };

    for (const QString &item : navItems) {
        m_sidebarNav->addItem(item);
    }

    // Set "Daily Entry" as default selected
    m_sidebarNav->setCurrentRow(1);
}

// ----------------------------------------------------------------------------
// Content Area Header
// ----------------------------------------------------------------------------
void MainWindow::setupContentArea()
{
    m_pageTitle = new QLabel("Daily Transactions");
    m_pageTitle->setObjectName("pageTitle");

    m_pageSubtitle = new QLabel("Database: finlogs • FY: 2026-2027");
    m_pageSubtitle->setObjectName("pageSubtitle");

    // Search bar (placed in table header area)
    m_searchBar = new QLineEdit;
    m_searchBar->setObjectName("searchBar");
    m_searchBar->setPlaceholderText("Search...");
    m_searchBar->setFixedWidth(220);
    m_searchBar->setClearButtonEnabled(true);
}

// ----------------------------------------------------------------------------
// Input Form (Date, Bill No, Party, Type, Mode, Amount, Buttons)
// ----------------------------------------------------------------------------
void MainWindow::setupInputForm()
{
    m_inputFormContainer = new QFrame;
    m_inputFormContainer->setObjectName("inputFormContainer");
    m_inputFormContainer->setFrameShape(QFrame::NoFrame);

    QGridLayout *formGrid = new QGridLayout(m_inputFormContainer);
    formGrid->setContentsMargins(16, 16, 16, 16);
    formGrid->setHorizontalSpacing(16);
    formGrid->setVerticalSpacing(8);

    // Row 0: Labels
    formGrid->addWidget(new QLabel("Date"),     0, 0);
    formGrid->addWidget(new QLabel("Bill No."), 0, 1);
    formGrid->addWidget(new QLabel("Party"),    0, 2, 1, 2); // span 2 cols

    // Row 1: Date, Bill No, Party
    m_dateEdit = new QDateEdit(QDate::currentDate());
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("dd-MM-yyyy");
    formGrid->addWidget(m_dateEdit, 1, 0);

    m_billNoEdit = new QLineEdit;
    m_billNoEdit->setPlaceholderText("Bill No.");
    formGrid->addWidget(m_billNoEdit, 1, 1);

    m_partyEdit = new QLineEdit;
    m_partyEdit->setPlaceholderText("Select an existing or create new from Add Party");
    formGrid->addWidget(m_partyEdit, 1, 2, 1, 2);

    // Row 2: Labels for Type, Mode, Amount
    formGrid->addWidget(new QLabel("Type"), 2, 0);
    formGrid->addWidget(new QLabel("Mode"), 2, 1);
    formGrid->addWidget(new QLabel("Amount"), 2, 2);

    // Row 3: Type, Mode, Amount, Save, Clear
    m_typeCombo = new QComboBox;
    m_typeCombo->addItems({"Sale", "Purchase", "Receipt", "Payment"});
    formGrid->addWidget(m_typeCombo, 3, 0);

    m_modeCombo = new QComboBox;
    m_modeCombo->addItems({"Cash", "Credit", "UPI", "Bank Transfer"});
    formGrid->addWidget(m_modeCombo, 3, 1);

    m_amountSpin = new QDoubleSpinBox;
    m_amountSpin->setRange(0.0, 99999999.99);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setPrefix("$");
    m_amountSpin->setValue(0.00);
    m_amountSpin->setGroupSeparatorShown(true);
    formGrid->addWidget(m_amountSpin, 3, 2);

    // Buttons
    m_btnSave = new QPushButton("Save Entry");
    m_btnSave->setObjectName("btnSaveEntry");
    formGrid->addWidget(m_btnSave, 3, 3);

    m_btnClear = new QPushButton("Clear");
    m_btnClear->setObjectName("btnClear");
    formGrid->addWidget(m_btnClear, 3, 4);

    // Column stretch: Party field should be wider
    formGrid->setColumnStretch(0, 1);
    formGrid->setColumnStretch(1, 1);
    formGrid->setColumnStretch(2, 2);
    formGrid->setColumnStretch(3, 0);
    formGrid->setColumnStretch(4, 0);
}

// ----------------------------------------------------------------------------
// Data Table (QTableWidget)
// ----------------------------------------------------------------------------
void MainWindow::setupTable()
{
    m_table = new QTableWidget;
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Disable vertical grid lines, keep only faint horizontal separators (via QSS)
    m_table->setShowGrid(false);

    // Remove the dotted focus rectangle
    m_table->setFocusPolicy(Qt::StrongFocus);
    m_table->setStyleSheet(
        "QTableWidget { outline: none; }"
        "QTableWidget::item:focus { border: none; outline: none; }"
    );

    // Columns
    QStringList headers = {"Date", "Bill No.", "Party", "Type", "Mode", "Amount"};
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);

    // --- Header configuration ---
    QHeaderView *hHeader = m_table->horizontalHeader();
    hHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hHeader->setSectionResizeMode(QHeaderView::Stretch);
    hHeader->setHighlightSections(false);
    hHeader->setStretchLastSection(true);
    hHeader->setMinimumSectionSize(80);

    // --- CRITICAL: Right-align the "Amount" column header ---
    // The Amount column is index 5
    QTableWidgetItem *amountHeader = m_table->horizontalHeaderItem(5);
    if (amountHeader) {
        amountHeader->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    // Vertical header (row numbers) - hide it for cleaner look
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(40);

    // Populate with sample data
    populateSampleData();
}

// ----------------------------------------------------------------------------
// Sample Data (mirrors your screenshot)
// ----------------------------------------------------------------------------
void MainWindow::populateSampleData()
{
    struct Entry {
        QString date;
        QString billNo;
        QString party;
        QString type;
        QString mode;
        double  amount;
    };

    QList<Entry> entries = {
        {"2026-05-26", "22",  "dsdsd",    "Receipt",  "Cash",   2222222.00},
        {"2026-05-26", "25",  "dsdsd",    "Sale",     "Credit", 250.00},
        {"2026-05-26", "s",   "dsdsd",    "Sale",     "Credit", 455.00},
        {"2026-05-26", "25",  "WHOAMI",   "Purchase", "Credit", 25.00},
        {"2026-05-26", "dff", "Customer", "Sale",     "Cash",   2000.00},
        {"2026-05-26", "dff", "Customer", "Sale",     "Cash",   2000.00},
        {"2026-05-26", "dff", "Customer", "Sale",     "Cash",   2000.00},
        {"2026-05-26", "dff", "Customer", "Sale",     "Cash",   2000.00},
    };

    m_table->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const Entry &e = entries[row];

        auto *dateItem  = new QTableWidgetItem(e.date);
        auto *billItem  = new QTableWidgetItem(e.billNo);
        auto *partyItem = new QTableWidgetItem(e.party);
        auto *typeItem  = new QTableWidgetItem(e.type);
        auto *modeItem  = new QTableWidgetItem(e.mode);
        auto *amtItem   = new QTableWidgetItem(
            QString("$%L1").arg(e.amount, 0, 'f', 2)
        );

        // Left-align all columns except Amount
        dateItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        billItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        partyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        typeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        modeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // --- CRITICAL: Right-align the Amount column ---
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_table->setItem(row, 0, dateItem);
        m_table->setItem(row, 1, billItem);
        m_table->setItem(row, 2, partyItem);
        m_table->setItem(row, 3, typeItem);
        m_table->setItem(row, 4, modeItem);
        m_table->setItem(row, 5, amtItem);
    }
}
