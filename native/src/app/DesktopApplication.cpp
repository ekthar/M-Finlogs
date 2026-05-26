#include "app/DesktopApplication.h"

#include "domain/Types.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QParallelAnimationGroup>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QToolBar>
#include <QVariant>
#include <QVBoxLayout>
#include <QTimer>

#include <exception>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <functional>

namespace {

class DefaultPartyTextFilter final : public QObject {
public:
    explicit DefaultPartyTextFilter(QLineEdit& input)
        : QObject(&input), input_(input) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != &input_) {
            return QObject::eventFilter(watched, event);
        }
        if (event->type() == QEvent::FocusIn) {
            if (input_.text() == QStringLiteral("Customer")) {
                input_.clear();
            }
            input_.selectAll();
        }
        if (event->type() == QEvent::FocusOut && input_.text().trimmed().isEmpty()) {
            input_.setText(QStringLiteral("Customer"));
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QLineEdit& input_;
};

class InventoryTableKeyFilter final : public QObject {
public:
    explicit InventoryTableKeyFilter(QTableWidget& table)
        : QObject(&table), table_(table) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != &table_ || event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(watched, event);
        }

        const QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            moveToNextEditableCell();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void moveToNextEditableCell() {
        int row = table_.currentRow();
        int column = table_.currentColumn();
        if (row < 0 || column < 0) {
            table_.setCurrentCell(0, 1);
            return;
        }

        for (int step = 0; step < table_.rowCount() * table_.columnCount(); step += 1) {
            column += 1;
            if (column >= table_.columnCount()) {
                column = 1;
                row += 1;
            }
            if (row >= table_.rowCount()) {
                row = 0;
            }
            if (!table_.isColumnHidden(column) && table_.item(row, column) && table_.item(row, column)->flags().testFlag(Qt::ItemIsEditable)) {
                table_.setCurrentCell(row, column);
                table_.editItem(table_.item(row, column));
                return;
            }
        }
    }

    QTableWidget& table_;
};

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

QFrame* createAccentPanel(QWidget* parent) {
    QFrame* panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("accentPanel"));
    panel->setFrameShape(QFrame::NoFrame);
    return panel;
}

QString currentCompanyText(mfinlogs::app::AppContext& context) {
    const mfinlogs::domain::DatabaseConfig config = context.services().config->readDatabaseConfig();
    return QStringLiteral("Database: %1").arg(config.database);
}

QString currentFinancialYearText() {
    const QDate today = QDate::currentDate();
    const int startYear = today.month() >= 4 ? today.year() : today.year() - 1;
    return QStringLiteral("FY: %1-%2").arg(startYear).arg(startYear + 1);
}

QString currentFinancialYearValue() {
    const QDate today = QDate::currentDate();
    const int startYear = today.month() >= 4 ? today.year() : today.year() - 1;
    return QStringLiteral("%1-%2").arg(startYear).arg(startYear + 1);
}

QFrame* createContextBar(mfinlogs::app::AppContext& context, QWidget* parent) {
    QFrame* bar = new QFrame(parent);
    bar->setObjectName(QStringLiteral("contextBar"));
    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    QLabel* company = new QLabel(currentCompanyText(context), bar);
    QLabel* year = new QLabel(currentFinancialYearText(), bar);
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
    table->verticalHeader()->setDefaultSectionSize(34);
    table->horizontalHeader()->setMinimumHeight(38);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setShowGrid(true);
    table->setGridStyle(Qt::SolidLine);
    table->setWordWrap(false);
    table->setCornerButtonEnabled(false);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    return table;
}

QTableWidget* createInventoryTable(QWidget* parent) {
    QTableWidget* table = createDataTable(parent);
    table->setObjectName(QStringLiteral("inventoryTable"));
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(36);
    table->installEventFilter(new InventoryTableKeyFilter(*table));
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
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);
    QLabel* titleLabel = new QLabel(label, tile);
    QLabel* valueLabel = new QLabel(value, tile);
    titleLabel->setObjectName(QStringLiteral("metricLabel"));
    valueLabel->setObjectName(QStringLiteral("metricValue"));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return tile;
}

QString findExistingPartyName(const QStringList& existingNames, const QString& typedName) {
    const QString normalizedTypedName = typedName.trimmed();
    for (const QString& existingName : existingNames) {
        if (existingName.compare(normalizedTypedName, Qt::CaseInsensitive) == 0) {
            return existingName;
        }
    }
    return QString();
}

int daysInInventoryMonth(int month) {
    const QDate today = QDate::currentDate();
    const int calendarYear = month >= 4 ? today.year() : today.year() + 1;
    return QDate(calendarYear, month, 1).daysInMonth();
}

QString inventoryDateRangeText(const QString& financialYear, int month) {
    const QStringList yearParts = financialYear.split(QLatin1Char('-'));
    const int startYear = yearParts.isEmpty() ? QDate::currentDate().year() : yearParts.first().toInt();
    const int calendarYear = month >= 4 ? startYear : startYear + 1;
    const QDate start(calendarYear, month, 1);
    return QStringLiteral("%1 to %2").arg(
        start.toString(QStringLiteral("dd-MM-yyyy")),
        QDate(calendarYear, month, start.daysInMonth()).toString(QStringLiteral("dd-MM-yyyy"))
    );
}

QTableWidgetItem* createTableItem(const QString& text, Qt::ItemFlags flags) {
    QTableWidgetItem* item = new QTableWidgetItem(text);
    item->setFlags(flags);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return item;
}

QTableWidgetItem* createNumberTableItem(const QString& text, Qt::ItemFlags flags) {
    QTableWidgetItem* item = createTableItem(text, flags);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

void animatePanel(QWidget& widget, int delayMs) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(&widget);
    effect->setOpacity(0.0);
    widget.setGraphicsEffect(effect);
    QPropertyAnimation* fade = new QPropertyAnimation(effect, "opacity", &widget);
    fade->setDuration(420);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(delayMs, fade, [fade]() { fade->start(QAbstractAnimation::DeleteWhenStopped); });
}

QString moneyText(double value) {
    return QStringLiteral("%1").arg(value, 0, 'f', 2);
}

QString transactionTypeText(mfinlogs::domain::TransactionType type) {
    switch (type) {
    case mfinlogs::domain::TransactionType::Sale:
        return QStringLiteral("Sale");
    case mfinlogs::domain::TransactionType::SaleReturn:
        return QStringLiteral("Sale Return");
    case mfinlogs::domain::TransactionType::Purchase:
        return QStringLiteral("Purchase");
    case mfinlogs::domain::TransactionType::Expense:
        return QStringLiteral("Expense");
    case mfinlogs::domain::TransactionType::Receipt:
        return QStringLiteral("Receipt");
    }
    return QStringLiteral("Sale");
}

QString paymentModeText(mfinlogs::domain::PaymentMode mode) {
    switch (mode) {
    case mfinlogs::domain::PaymentMode::Credit:
        return QStringLiteral("Credit");
    case mfinlogs::domain::PaymentMode::Cash:
        return QStringLiteral("Cash");
    case mfinlogs::domain::PaymentMode::Upi:
        return QStringLiteral("UPI");
    case mfinlogs::domain::PaymentMode::Bank:
        return QStringLiteral("Bank");
    }
    return QStringLiteral("Cash");
}

mfinlogs::domain::TransactionType transactionTypeFromText(const QString& text) {
    if (text == QStringLiteral("Sale")) {
        return mfinlogs::domain::TransactionType::Sale;
    }
    if (text == QStringLiteral("Sale Return")) {
        return mfinlogs::domain::TransactionType::SaleReturn;
    }
    if (text == QStringLiteral("Purchase")) {
        return mfinlogs::domain::TransactionType::Purchase;
    }
    if (text == QStringLiteral("Expense")) {
        return mfinlogs::domain::TransactionType::Expense;
    }
    return mfinlogs::domain::TransactionType::Receipt;
}

mfinlogs::domain::PaymentMode paymentModeFromText(const QString& text) {
    if (text == QStringLiteral("Credit")) {
        return mfinlogs::domain::PaymentMode::Credit;
    }
    if (text == QStringLiteral("UPI")) {
        return mfinlogs::domain::PaymentMode::Upi;
    }
    if (text == QStringLiteral("Bank")) {
        return mfinlogs::domain::PaymentMode::Bank;
    }
    return mfinlogs::domain::PaymentMode::Cash;
}

QJsonObject transactionToJson(const mfinlogs::domain::TransactionRow& row) {
    QJsonObject item;
    item.insert(QStringLiteral("id"), row.id);
    item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
    item.insert(QStringLiteral("bill_no"), row.billNo);
    item.insert(QStringLiteral("party"), row.party);
    item.insert(QStringLiteral("amount"), row.amount);

    item.insert(QStringLiteral("type"), transactionTypeText(row.type));
    item.insert(QStringLiteral("mode"), paymentModeText(row.mode));

    return item;
}

QString incrementBillNumber(const QString& billNo) {
    if (billNo.trimmed().isEmpty()) {
        return QString();
    }

    int index = billNo.size() - 1;
    while (index >= 0 && billNo.at(index).isDigit()) {
        index -= 1;
    }
    if (index == billNo.size() - 1) {
        return billNo;
    }

    const QString prefix = billNo.left(index + 1);
    const QString numberText = billNo.mid(index + 1);
    const QString nextNumber = QString::number(numberText.toLongLong() + 1).rightJustified(numberText.size(), QLatin1Char('0'));
    return prefix + nextNumber;
}

int rangeDays(const QDate& start, const QDate& end) {
    return static_cast<int>(std::max<qint64>(1, start.daysTo(end) + 1));
}

bool rowContainsText(const QTableWidget& table, int rowIndex, const QString& text) {
    for (int columnIndex = 0; columnIndex < table.columnCount(); columnIndex += 1) {
        const QTableWidgetItem* item = table.item(rowIndex, columnIndex);
        if (item && item->text().contains(text, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QShortcut* createShortcut(QWidget& owner, const QKeySequence& sequence, const std::function<void()>& action) {
    QShortcut* shortcut = new QShortcut(sequence, &owner);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(shortcut, &QShortcut::activated, &owner, action);
    return shortcut;
}

void focusEntryWidget(QWidget& widget) {
    widget.setFocus(Qt::ShortcutFocusReason);
    if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(&widget)) {
        lineEdit->selectAll();
    } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(&widget)) {
        spinBox->selectAll();
    } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(&widget)) {
        comboBox->showPopup();
    }
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
    if (!showAuthDialog()) {
        QTimer::singleShot(0, qApp, &QApplication::quit);
    }
}

void DesktopApplication::applyTheme() {
    const QString fontDir = QCoreApplication::applicationDirPath() + QStringLiteral("/fonts/");
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Regular.ttf"));
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Bold.ttf"));
    qApp->setFont(QFont(QStringLiteral("Space Mono"), 10));
    qApp->setStyleSheet(QStringLiteral(
        "* { font-family: 'Space Mono', 'Consolas', 'Courier New', monospace; letter-spacing: 0; }"
        "QMainWindow, QWidget#workspace { background: #f7f1df; color: #0f2436; font-size: 12px; }"
        "QWidget#sidebarWrap { background: #082f63; border-right: 1px solid #06264f; }"
        "QListWidget#sidebar { background: transparent; border: 0; color: #dce9f8; padding: 10px 10px 14px 10px; outline: 0; }"
        "QListWidget#sidebar::item { border-radius: 6px; min-height: 32px; padding: 0 12px; margin: 2px 0; }"
        "QListWidget#sidebar::item:selected { background: #d7e8ff; color: #082f63; border: 1px solid #8fb8ea; }"
        "QListWidget#sidebar::item:hover:!selected { background: #12477f; color: #ffffff; }"
        "QListWidget#sidebar::item:disabled { color: #92abc8; font-size: 11px; font-weight: 700; padding-top: 12px; }"
        "QLabel#brand { color: #ffffff; font-size: 18px; font-weight: 900; padding: 20px 18px 2px 18px; }"
        "QLabel#brandSub { color: #b8cee7; font-size: 11px; padding: 0 18px 12px 18px; }"
        "QLabel#welcomeTitle { color: #082f63; font-size: 38px; font-weight: 900; }"
        "QLabel#welcomeKicker { color: #1767aa; font-size: 12px; font-weight: 900; }"
        "QLabel#welcomeStat { color: #6f5d38; font-size: 11px; font-weight: 700; }"
        "QLabel#pageTitle { color: #082f63; font-size: 23px; font-weight: 900; }"
        "QLabel#pageMeta { color: #58677a; font-size: 11px; }"
        "QLabel#sectionTitle { color: #082f63; font-size: 15px; font-weight: 900; }"
        "QLabel#sectionDescription { color: #58677a; line-height: 18px; }"
        "QLabel#metricLabel { color: #58677a; font-size: 11px; font-weight: 800; }"
        "QLabel#metricValue { color: #082f63; font-size: 20px; font-weight: 900; }"
        "QLabel#contextChip { background: #fffaf0; border: 1px solid #d7c8a8; border-radius: 6px; color: #24374a; padding: 6px 10px; }"
        "QFrame#contextBar { background: #fffaf0; border: 1px solid #d7c8a8; border-radius: 8px; }"
        "QFrame#panel { background: #fffaf0; border: 1px solid #d7c8a8; border-radius: 8px; }"
        "QFrame#accentPanel { background: #eaf3ff; border: 1px solid #8fb8ea; border-radius: 8px; }"
        "QFrame#welcomeHero { background: #fffaf0; border: 1px solid #d7c8a8; border-radius: 8px; }"
        "QTableWidget#dataTable, QTableWidget#inventoryTable { background: #fffaf0; alternate-background-color: #f4ead6; border: 1px solid #d7c8a8; border-radius: 8px; selection-background-color: #d7e8ff; selection-color: #082f63; gridline-color: #e2d4b8; color: #0f2436; }"
        "QHeaderView::section { background: #082f63; color: #fffaf0; border: 0; border-right: 1px solid #1b4c83; padding: 8px; font-weight: 900; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #eadcc2; color: #0f2436; }"
        "QTableWidget#inventoryTable::item { padding: 6px; border-right: 1px solid #eadcc2; border-bottom: 1px solid #eadcc2; }"
        "QTableWidget#inventoryTable::item:selected { background: #d7e8ff; color: #082f63; }"
        "QPushButton { background: #0b5cab; color: #fffaf0; border: 1px solid #073f78; border-radius: 6px; min-height: 30px; padding: 7px 13px; font-weight: 900; }"
        "QPushButton:hover { background: #074c91; }"
        "QPushButton#secondaryButton { background: #edf5ff; color: #082f63; border: 1px solid #8fb8ea; }"
        "QPushButton#secondaryButton:hover { background: #d7e8ff; }"
        "QLineEdit, QDateEdit, QDoubleSpinBox, QComboBox { background: #fffaf0; border: 1px solid #c9b68f; border-radius: 6px; min-height: 32px; padding: 0 10px; color: #0f2436; }"
        "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QComboBox:focus { background: #ffffff; border-color: #0b5cab; }"
        "QComboBox QAbstractItemView { background: #fffaf0; color: #0f2436; selection-background-color: #d7e8ff; selection-color: #082f63; border: 1px solid #8fb8ea; }"
        "QToolBar { background: #fffaf0; border-bottom: 1px solid #d7c8a8; spacing: 8px; padding: 7px; }"
        "QStatusBar { background: #fffaf0; color: #58677a; border-top: 1px solid #d7c8a8; }"
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
    pages->addWidget(buildWelcomePage(*pages, *nav));
    addNavItem(*nav, QStringLiteral("Welcome"), pageIndex);
    pageIndex += 1;
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
    pages->addWidget(buildInventoryPage());
    addNavItem(*nav, QStringLiteral("Inventory Management"), pageIndex);
    pageIndex += 1;
    pages->addWidget(buildInventoryValuePage());
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
            QWidget* page = pages->currentWidget();
            if (page) {
                animatePanel(*page, 0);
            }
        }
    });
    nav->setCurrentRow(0);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(pages, 1);
    setCentralWidget(root);
}

bool DesktopApplication::showAuthDialog() {
    try {
        const bool setupRequired = context_.services().auth->setupRequired();
        QDialog dialog(this);
        dialog.setWindowTitle(setupRequired ? QStringLiteral("Create Admin Password") : QStringLiteral("Sign In"));
        dialog.setModal(true);
        dialog.resize(420, 260);
        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(22, 20, 22, 20);
        layout->setSpacing(12);
        QLabel* title = new QLabel(setupRequired ? QStringLiteral("First run setup") : QStringLiteral("Sign in to continue"), &dialog);
        title->setObjectName(QStringLiteral("pageTitle"));
        QLabel* subtitle = new QLabel(setupRequired ? QStringLiteral("Create the admin password for this database.") : currentCompanyText(context_), &dialog);
        subtitle->setObjectName(QStringLiteral("pageMeta"));
        QLineEdit* username = new QLineEdit(setupRequired ? QStringLiteral("admin") : QString(), &dialog);
        username->setPlaceholderText(QStringLiteral("Username"));
        username->setEnabled(!setupRequired);
        QLineEdit* password = new QLineEdit(&dialog);
        password->setPlaceholderText(setupRequired ? QStringLiteral("Create password") : QStringLiteral("Password"));
        password->setEchoMode(QLineEdit::Password);
        QLabel* error = new QLabel(&dialog);
        error->setObjectName(QStringLiteral("pageMeta"));
        QPushButton* submit = new QPushButton(setupRequired ? QStringLiteral("Save Admin") : QStringLiteral("Sign In"), &dialog);
        QPushButton* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
        cancel->setObjectName(QStringLiteral("secondaryButton"));
        QHBoxLayout* actions = new QHBoxLayout();
        actions->addStretch(1);
        actions->addWidget(cancel);
        actions->addWidget(submit);
        layout->addWidget(title);
        layout->addWidget(subtitle);
        layout->addWidget(username);
        layout->addWidget(password);
        layout->addWidget(error);
        layout->addLayout(actions);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(password, &QLineEdit::returnPressed, submit, &QPushButton::click);
        connect(submit, &QPushButton::clicked, this, [this, setupRequired, username, password, error, &dialog]() {
            try {
                const QString user = setupRequired ? QStringLiteral("admin") : username->text().trimmed();
                if (setupRequired) {
                    context_.services().auth->setupAdmin(user, password->text());
                } else {
                    context_.services().auth->login(user, password->text());
                }
                dialog.accept();
            } catch (const std::exception& err) {
                error->setText(QString::fromUtf8(err.what()));
            }
        });
        QTimer::singleShot(0, password, [password]() { password->setFocus(); });
        return dialog.exec() == QDialog::Accepted;
    } catch (const std::exception& err) {
        showError(QStringLiteral("Authentication"), err);
        QMessageBox::warning(this, QStringLiteral("Authentication"), QStringLiteral("Could not load auth state. Open Settings and verify the database connection.\n\n%1").arg(QString::fromUtf8(err.what())));
        return true;
    }
}

QWidget* DesktopApplication::buildWelcomePage(QStackedWidget& pages, QListWidget& nav) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 28, 32, 32);
    layout->setSpacing(18);

    QFrame* hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("welcomeHero"));
    hero->setMinimumHeight(230);
    QGridLayout* heroLayout = new QGridLayout(hero);
    heroLayout->setContentsMargins(26, 24, 26, 24);
    heroLayout->setHorizontalSpacing(22);
    heroLayout->setVerticalSpacing(12);
    QLabel* kicker = new QLabel(QStringLiteral("M-FINLOGS NATIVE WORKSPACE"), hero);
    kicker->setObjectName(QStringLiteral("welcomeKicker"));
    QLabel* title = new QLabel(QStringLiteral("Welcome back"), hero);
    title->setObjectName(QStringLiteral("welcomeTitle"));
    QLabel* subtitle = new QLabel(currentCompanyText(context_) + QStringLiteral("  |  ") + currentFinancialYearText(), hero);
    subtitle->setObjectName(QStringLiteral("pageMeta"));
    QLabel* statOne = new QLabel(QStringLiteral("Keyboard first entry"), hero);
    QLabel* statTwo = new QLabel(QStringLiteral("SQL backed ledgers"), hero);
    QLabel* statThree = new QLabel(QStringLiteral("Packaged native runtime"), hero);
    statOne->setObjectName(QStringLiteral("welcomeStat"));
    statTwo->setObjectName(QStringLiteral("welcomeStat"));
    statThree->setObjectName(QStringLiteral("welcomeStat"));
    heroLayout->addWidget(kicker, 0, 0, 1, 3);
    heroLayout->addWidget(title, 1, 0, 1, 3);
    heroLayout->addWidget(subtitle, 2, 0, 1, 3);
    heroLayout->addWidget(statOne, 0, 3);
    heroLayout->addWidget(statTwo, 1, 3);
    heroLayout->addWidget(statThree, 2, 3);

    QWidget* actionRow = new QWidget(hero);
    QHBoxLayout* actions = new QHBoxLayout(actionRow);
    actions->setContentsMargins(0, 10, 0, 0);
    actions->setSpacing(10);
    QPushButton* entry = new QPushButton(QStringLiteral("Daily Entry"), hero);
    QPushButton* reports = new QPushButton(QStringLiteral("Reports"), hero);
    QPushButton* inventory = new QPushButton(QStringLiteral("Inventory"), hero);
    QPushButton* settings = new QPushButton(QStringLiteral("Settings"), hero);
    entry->setMinimumWidth(132);
    reports->setMinimumWidth(120);
    inventory->setMinimumWidth(132);
    settings->setMinimumWidth(120);
    reports->setObjectName(QStringLiteral("secondaryButton"));
    inventory->setObjectName(QStringLiteral("secondaryButton"));
    settings->setObjectName(QStringLiteral("secondaryButton"));
    actions->addWidget(entry);
    actions->addWidget(reports);
    actions->addWidget(inventory);
    actions->addWidget(settings);
    actions->addStretch(1);
    heroLayout->addWidget(actionRow, 3, 0, 1, 4);
    layout->addWidget(hero);

    QGridLayout* metrics = new QGridLayout();
    metrics->setSpacing(14);
    loadDashboard(*metrics);
    layout->addLayout(metrics);

    QFrame* flow = createAccentPanel(page);
    QGridLayout* flowLayout = new QGridLayout(flow);
    flowLayout->setContentsMargins(18, 18, 18, 18);
    flowLayout->setSpacing(14);
    flowLayout->addWidget(createMetricTile(QStringLiteral("Keyboard Entry"), QStringLiteral("Enter flow active"), flow), 0, 0);
    flowLayout->addWidget(createMetricTile(QStringLiteral("Inventory"), QStringLiteral("SQL snapshots"), flow), 0, 1);
    flowLayout->addWidget(createMetricTile(QStringLiteral("Backups"), QStringLiteral("Packaged runtime"), flow), 0, 2);
    layout->addWidget(flow);
    layout->addStretch(1);

    auto goToPage = [&pages, &nav](int pageIndex) {
        for (int row = 0; row < nav.count(); row += 1) {
            QListWidgetItem* item = nav.item(row);
            if (item && item->data(Qt::UserRole).toInt() == pageIndex) {
                nav.setCurrentRow(row);
                return;
            }
        }
        pages.setCurrentIndex(pageIndex);
    };
    connect(entry, &QPushButton::clicked, this, [goToPage]() { goToPage(1); });
    connect(reports, &QPushButton::clicked, this, [goToPage]() { goToPage(3); });
    connect(inventory, &QPushButton::clicked, this, [goToPage]() { goToPage(12); });
    connect(settings, &QPushButton::clicked, this, [goToPage, &pages]() { goToPage(pages.count() - 1); });

    animatePanel(*hero, 0);
    animatePanel(*flow, 180);
    return page;
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
    layout->addWidget(createContextBar(context_, page));

    QFrame* entryPanel = createPanel(page);
    QGridLayout* form = new QGridLayout(entryPanel);
    form->setContentsMargins(16, 16, 16, 16);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(12);

    QDateEdit* date = new QDateEdit(QDate::currentDate(), entryPanel);
    date->setCalendarPopup(true);
    QLineEdit* bill = new QLineEdit(entryPanel);
    bill->setPlaceholderText(QStringLiteral("Bill No."));
    QLineEdit* party = new QLineEdit(QStringLiteral("Customer"), entryPanel);
    QLabel* partyHint = new QLabel(QStringLiteral("Select an existing party. Create new names from Add Party."), entryPanel);
    partyHint->setObjectName(QStringLiteral("pageMeta"));
    party->installEventFilter(new DefaultPartyTextFilter(*party));
    try {
        QCompleter* completer = new QCompleter(partyNames(), party);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        party->setCompleter(completer);
    } catch (const std::exception& err) {
        showError(QStringLiteral("Party Suggestions"), err);
    }
    QComboBox* type = new QComboBox(entryPanel);
    type->addItems({QStringLiteral("Sale"), QStringLiteral("Sale Return"), QStringLiteral("Expense"), QStringLiteral("Receipt"), QStringLiteral("Purchase")});
    QComboBox* mode = new QComboBox(entryPanel);
    mode->addItems({QStringLiteral("Credit"), QStringLiteral("Cash"), QStringLiteral("UPI"), QStringLiteral("Bank")});
    QDoubleSpinBox* amount = new QDoubleSpinBox(entryPanel);
    amount->setMaximum(999999999.0);
    amount->setDecimals(2);
    QPushButton* save = new QPushButton(QStringLiteral("Save Entry"), entryPanel);
    QPushButton* deleteButton = new QPushButton(QStringLiteral("Delete"), entryPanel);
    deleteButton->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* clear = new QPushButton(QStringLiteral("Clear"), entryPanel);
    clear->setObjectName(QStringLiteral("secondaryButton"));

    form->addWidget(new QLabel(QStringLiteral("Date"), entryPanel), 0, 0);
    form->addWidget(date, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("Bill No."), entryPanel), 0, 1);
    form->addWidget(bill, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Party"), entryPanel), 0, 2);
    form->addWidget(party, 1, 2, 1, 2);
    form->addWidget(partyHint, 2, 2, 1, 2);
    form->addWidget(new QLabel(QStringLiteral("Type"), entryPanel), 3, 0);
    form->addWidget(type, 4, 0);
    form->addWidget(new QLabel(QStringLiteral("Mode"), entryPanel), 3, 1);
    form->addWidget(mode, 4, 1);
    form->addWidget(new QLabel(QStringLiteral("Amount"), entryPanel), 3, 2);
    form->addWidget(amount, 4, 2);
    form->addWidget(save, 4, 3);
    form->addWidget(deleteButton, 4, 4);
    form->addWidget(clear, 4, 5);
    form->setColumnStretch(0, 1);
    form->setColumnStretch(1, 2);
    form->setColumnStretch(2, 2);
    form->setColumnStretch(3, 1);
    form->setColumnStretch(4, 1);
    form->setColumnStretch(5, 1);
    layout->addWidget(entryPanel);

    createShortcut(*date, QKeySequence(Qt::Key_Return), [bill]() { focusEntryWidget(*bill); });
    createShortcut(*date, QKeySequence(Qt::Key_Enter), [bill]() { focusEntryWidget(*bill); });
    createShortcut(*bill, QKeySequence(Qt::Key_Return), [party]() { focusEntryWidget(*party); });
    createShortcut(*bill, QKeySequence(Qt::Key_Enter), [party]() { focusEntryWidget(*party); });
    createShortcut(*party, QKeySequence(Qt::Key_Return), [type]() { focusEntryWidget(*type); });
    createShortcut(*party, QKeySequence(Qt::Key_Enter), [type]() { focusEntryWidget(*type); });
    createShortcut(*type, QKeySequence(Qt::Key_Return), [mode]() { focusEntryWidget(*mode); });
    createShortcut(*type, QKeySequence(Qt::Key_Enter), [mode]() { focusEntryWidget(*mode); });
    createShortcut(*mode, QKeySequence(Qt::Key_Return), [amount]() { focusEntryWidget(*amount); });
    createShortcut(*mode, QKeySequence(Qt::Key_Enter), [amount]() { focusEntryWidget(*amount); });
    createShortcut(*amount, QKeySequence(Qt::Key_Return), [save]() { save->click(); });
    createShortcut(*amount, QKeySequence(Qt::Key_Enter), [save]() { save->click(); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("F1")), [type]() { type->setCurrentText(QStringLiteral("Sale")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("F2")), [type]() { type->setCurrentText(QStringLiteral("Sale Return")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("F3")), [type]() { type->setCurrentText(QStringLiteral("Expense")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("F4")), [type]() { type->setCurrentText(QStringLiteral("Receipt")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("F5")), [type]() { type->setCurrentText(QStringLiteral("Purchase")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("Ctrl+1")), [mode]() { mode->setCurrentText(QStringLiteral("Credit")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("Ctrl+2")), [mode]() { mode->setCurrentText(QStringLiteral("Cash")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("Ctrl+3")), [mode]() { mode->setCurrentText(QStringLiteral("UPI")); });
    createShortcut(*entryPanel, QKeySequence(QStringLiteral("Ctrl+4")), [mode]() { mode->setCurrentText(QStringLiteral("Bank")); });

    connect(party, &QLineEdit::editingFinished, this, [party]() {
        if (party->text().trimmed().isEmpty()) {
            party->setText(QStringLiteral("Customer"));
        }
    });

    QFrame* tablePanel = createPanel(page);
    QVBoxLayout* tableLayout = new QVBoxLayout(tablePanel);
    QHBoxLayout* tableHeader = new QHBoxLayout();
    tableHeader->addWidget(createSectionTitle(QStringLiteral("Recent Entries"), tablePanel));
    tableHeader->addStretch(1);
    QLineEdit* transactionSearch = new QLineEdit(tablePanel);
    transactionSearch->setPlaceholderText(QStringLiteral("Search transactions..."));
    transactionSearch->setFixedWidth(280);
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), tablePanel);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    tableHeader->addWidget(transactionSearch);
    tableHeader->addWidget(refresh);
    tableLayout->addLayout(tableHeader);
    QTableWidget* table = createDataTable(tablePanel);
    loadTransactions(*table);
    tableLayout->addWidget(table);
    connect(transactionSearch, &QLineEdit::textChanged, this, [this, table](const QString& query) { applyTableSearch(*table, query); });
    connect(refresh, &QPushButton::clicked, this, [this, table, transactionSearch]() {
        loadTransactions(*table);
        applyTableSearch(*table, transactionSearch->text());
    });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+S")), [save]() { save->click(); });
    createShortcut(*page, QKeySequence(QStringLiteral("Alt+N")), [date]() { focusEntryWidget(*date); });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+K")), [transactionSearch]() { focusEntryWidget(*transactionSearch); });
    const std::shared_ptr<int> editingId = std::make_shared<int>(0);
    connect(clear, &QPushButton::clicked, this, [date, bill, party, type, mode, amount, save, editingId]() {
        *editingId = 0;
        date->setDate(QDate::currentDate());
        bill->clear();
        party->setText(QStringLiteral("Customer"));
        type->setCurrentIndex(0);
        mode->setCurrentIndex(0);
        amount->setValue(0.0);
        save->setText(QStringLiteral("Save Entry"));
        focusEntryWidget(*bill);
    });
    connect(table, &QTableWidget::itemSelectionChanged, this, [this, table, date, bill, party, type, mode, amount, save, editingId]() {
        const QList<QTableWidgetItem*> selected = table->selectedItems();
        if (selected.isEmpty()) {
            return;
        }
        const int rowIndex = selected.first()->row();
        const QTableWidgetItem* idItem = table->item(rowIndex, 0);
        if (!idItem) {
            return;
        }
        const int transactionId = idItem->data(Qt::UserRole).toInt();
        if (transactionId <= 0) {
            return;
        }
        try {
            const domain::TransactionRow row = context_.services().transactions->getTransaction(transactionId);
            *editingId = transactionId;
            date->setDate(row.date);
            bill->setText(row.billNo);
            party->setText(row.party);
            type->setCurrentText(transactionTypeText(row.type));
            mode->setCurrentText(paymentModeText(row.mode));
            amount->setValue(row.amount);
            save->setText(QStringLiteral("Update Entry"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Load Entry"), err);
        }
    });
    connect(save, &QPushButton::clicked, this, [this, date, bill, party, type, mode, amount, table, editingId, save, transactionSearch]() {
        try {
            const QString typedPartyName = party->text().trimmed();
            if (typedPartyName.isEmpty() || typedPartyName == QStringLiteral("Customer")) {
                throw std::invalid_argument("Select an existing party");
            }

            const QString partyName = findExistingPartyName(partyNames(), typedPartyName);
            if (partyName.isEmpty()) {
                throw std::invalid_argument("Party does not exist. Create it from Add Party before saving a transaction.");
            }
            if (amount->value() <= 0.0) {
                throw std::invalid_argument("Amount must be greater than zero");
            }
            party->setText(partyName);

            const QString currentBill = bill->text().trimmed();
            if (*editingId > 0) {
                const QString adminUser = QStringLiteral("native-admin");
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("txn_date"), date->date().toString(Qt::ISODate), adminUser});
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("bill_no"), bill->text().trimmed(), adminUser});
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("party"), partyName, adminUser});
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("txn_type"), type->currentText(), adminUser});
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("payment_mode"), mode->currentText(), adminUser});
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{*editingId, QStringLiteral("amount"), QString::number(amount->value(), 'f', 2), adminUser});
                statusBar()->showMessage(QStringLiteral("Transaction updated"), 5000);
            } else {
                context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
                    date->date(),
                    bill->text().trimmed(),
                    partyName,
                    transactionTypeFromText(type->currentText()),
                    paymentModeFromText(mode->currentText()),
                    amount->value()
                });
                statusBar()->showMessage(QStringLiteral("Transaction saved"), 5000);
                bill->setText(incrementBillNumber(currentBill));
                party->setText(QStringLiteral("Customer"));
                type->setCurrentText(QStringLiteral("Sale"));
                mode->setCurrentText(QStringLiteral("Credit"));
                amount->setValue(0.0);
                focusEntryWidget(*bill);
            }
            loadTransactions(*table);
            applyTableSearch(*table, transactionSearch->text());
            *editingId = 0;
            save->setText(QStringLiteral("Save Entry"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Save Entry"), err);
        }
    });
    connect(deleteButton, &QPushButton::clicked, this, [this, table, editingId, save, transactionSearch]() {
        if (*editingId <= 0) {
            statusBar()->showMessage(QStringLiteral("Select a transaction to delete"), 5000);
            return;
        }
        const QMessageBox::StandardButton result = QMessageBox::question(
            this,
            QStringLiteral("Delete Transaction"),
            QStringLiteral("Delete selected transaction?"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result != QMessageBox::Yes) {
            return;
        }
        try {
            context_.services().transactions->deleteTransaction(domain::TransactionDeleteRequest{*editingId, QStringLiteral("native-admin")});
            *editingId = 0;
            save->setText(QStringLiteral("Save Entry"));
            loadTransactions(*table);
            applyTableSearch(*table, transactionSearch->text());
            statusBar()->showMessage(QStringLiteral("Transaction deleted"), 5000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Delete Entry"), err);
        }
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
    layout->addWidget(createContextBar(context_, page));

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
    layout->addWidget(createContextBar(context_, page));

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
    layout->addWidget(formPanel);

    QTableWidget* table = createDataTable(page);
    loadParties(*table);
    connect(create, &QPushButton::clicked, this, [this, name, type, table]() {
        try {
            const QString partyName = name->text().trimmed();
            if (partyName.isEmpty()) {
                throw std::invalid_argument("Party name is required");
            }
            const bool creditAllowed = type->currentText() == QStringLiteral("Credit Customer");
            context_.services().parties->createParty(partyName, type->currentText(), creditAllowed);
            name->clear();
            loadParties(*table);
            statusBar()->showMessage(QStringLiteral("Party created"), 5000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Create Party"), err);
        }
    });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildReportPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);

    layout->addWidget(createPageHeader(title, description, page));
    layout->addWidget(createContextBar(context_, page));

    QFrame* filters = createPanel(page);
    QHBoxLayout* filterLayout = new QHBoxLayout(filters);
    QLineEdit* search = new QLineEdit(filters);
    search->setPlaceholderText(QStringLiteral("Party or table search..."));
    try {
        QCompleter* completer = new QCompleter(partyNames(), search);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        search->setCompleter(completer);
    } catch (const std::exception& err) {
        showError(QStringLiteral("Report Party Suggestions"), err);
    }
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

    if (title == QStringLiteral("Short / Excess")) {
        QFrame* cashPanel = createPanel(page);
        QHBoxLayout* cashLayout = new QHBoxLayout(cashPanel);
        QDateEdit* cashDate = new QDateEdit(QDate::currentDate(), cashPanel);
        cashDate->setCalendarPopup(true);
        QDoubleSpinBox* cashAmount = new QDoubleSpinBox(cashPanel);
        cashAmount->setMaximum(999999999.0);
        cashAmount->setDecimals(2);
        QPushButton* saveCash = new QPushButton(QStringLiteral("Save Cash In Hand"), cashPanel);
        saveCash->setObjectName(QStringLiteral("secondaryButton"));
        cashLayout->addWidget(new QLabel(QStringLiteral("Cash Date"), cashPanel));
        cashLayout->addWidget(cashDate);
        cashLayout->addWidget(new QLabel(QStringLiteral("Cash In Hand"), cashPanel));
        cashLayout->addWidget(cashAmount);
        cashLayout->addStretch(1);
        cashLayout->addWidget(saveCash);
        layout->addWidget(cashPanel);
        connect(saveCash, &QPushButton::clicked, this, [this, cashDate, cashAmount, refresh]() {
            try {
                context_.services().reports->saveCashInHand(cashDate->date(), cashAmount->value());
                refresh->click();
                statusBar()->showMessage(QStringLiteral("Cash in hand saved"), 7000);
            } catch (const std::exception& err) {
                showError(QStringLiteral("Save Cash In Hand"), err);
            }
        });
    }

    QTableWidget* table = createDataTable(page);
    const domain::ReportRange initialRange{start->date(), end->date(), rangeDays(start->date(), end->date())};
    loadReportTable(*table, title, search->text().trimmed(), initialRange);
    connect(search, &QLineEdit::textChanged, this, [this, table, title](const QString& query) {
        if (title == QStringLiteral("Party Ledger")) {
            return;
        }
        applyTableSearch(*table, query);
    });
    connect(refresh, &QPushButton::clicked, this, [this, table, title, search, start, end]() {
        const domain::ReportRange range{start->date(), end->date(), rangeDays(start->date(), end->date())};
        loadReportTable(*table, title, search->text().trimmed(), range);
        if (title != QStringLiteral("Party Ledger")) {
            applyTableSearch(*table, search->text());
        }
    });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildAuditPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Audit Logs"), QStringLiteral("Administrative activity and native service events."), page));
    layout->addWidget(createContextBar(context_, page));

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
    layout->addWidget(createContextBar(context_, page));

    QFrame* panel = createPanel(page);
    QGridLayout* form = new QGridLayout(panel);
    const domain::DatabaseConfig config = context_.services().config->readDatabaseConfig();
    QLineEdit* server = new QLineEdit(config.server, panel);
    QLineEdit* database = new QLineEdit(config.database, panel);
    QComboBox* authType = new QComboBox(panel);
    authType->addItems({QStringLiteral("Windows Authentication"), QStringLiteral("SQL Server Authentication")});
    authType->setCurrentIndex(config.useWindowsAuth ? 0 : 1);
    QLineEdit* username = new QLineEdit(config.username, panel);
    QLineEdit* password = new QLineEdit(config.password, panel);
    password->setEchoMode(QLineEdit::Password);
    QLineEdit* backupDir = new QLineEdit(config.backupDir, panel);
    QLineEdit* apiBase = new QLineEdit(config.apiBaseUrl, panel);
    QPushButton* test = new QPushButton(QStringLiteral("Test Connection"), panel);
    test->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* backup = new QPushButton(QStringLiteral("Backup Now"), panel);
    backup->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* restore = new QPushButton(QStringLiteral("Restore Backup"), panel);
    restore->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* save = new QPushButton(QStringLiteral("Save Config"), panel);

    form->addWidget(new QLabel(QStringLiteral("SQL Server Instance"), panel), 0, 0);
    form->addWidget(server, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("Database Name"), panel), 0, 1);
    form->addWidget(database, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Authentication"), panel), 0, 2);
    form->addWidget(authType, 1, 2);
    form->addWidget(new QLabel(QStringLiteral("SQL Username"), panel), 2, 0);
    form->addWidget(username, 3, 0);
    form->addWidget(new QLabel(QStringLiteral("SQL Password"), panel), 2, 1);
    form->addWidget(password, 3, 1);
    form->addWidget(new QLabel(QStringLiteral("API Base"), panel), 2, 2);
    form->addWidget(apiBase, 3, 2);
    form->addWidget(new QLabel(QStringLiteral("Backup Directory"), panel), 4, 0);
    form->addWidget(backupDir, 5, 0, 1, 2);
    form->addWidget(test, 5, 2);
    form->addWidget(save, 5, 3);
    form->addWidget(backup, 6, 0);
    form->addWidget(restore, 6, 1);

    auto buildConfig = [server, database, authType, username, password, backupDir, apiBase, config]() {
        return domain::DatabaseConfig{
            server->text().trimmed(),
            database->text().trimmed(),
            username->text().trimmed(),
            password->text(),
            config.driver,
            apiBase->text().trimmed(),
            backupDir->text().trimmed(),
            authType->currentIndex() == 0
        };
    };

    connect(test, &QPushButton::clicked, this, [this, buildConfig]() {
        try {
            context_.services().config->testDatabaseConfig(buildConfig());
            statusBar()->showMessage(QStringLiteral("SQL connection successful"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Database Test"), err);
        }
    });
    connect(save, &QPushButton::clicked, this, [this, buildConfig]() {
        try {
            const domain::DatabaseConfig nextConfig = buildConfig();
            context_.services().config->testDatabaseConfig(nextConfig);
            context_.services().config->writeDatabaseConfig(nextConfig);
            statusBar()->showMessage(QStringLiteral("Database configuration saved. Restart native app to reload all screens."), 9000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Save Database Config"), err);
        }
    });
    connect(backup, &QPushButton::clicked, this, [this, backupDir]() {
        try {
            const domain::BackupResult result = context_.services().backups->backup(backupDir->text().trimmed());
            QMessageBox::information(this, QStringLiteral("Backup"), result.status + QStringLiteral("\n") + result.path);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Backup"), err);
        }
    });
    connect(restore, &QPushButton::clicked, this, [this, backupDir]() {
        try {
            const QString backupPath = QFileDialog::getOpenFileName(this, QStringLiteral("Choose SQL Backup"), backupDir->text().trimmed(), QStringLiteral("SQL Backup (*.bak)"));
            if (backupPath.trimmed().isEmpty()) {
                return;
            }
            const QMessageBox::StandardButton result = QMessageBox::question(
                this,
                QStringLiteral("Restore Backup"),
                QStringLiteral("Restore selected backup and replace the configured database?"),
                QMessageBox::Yes | QMessageBox::No
            );
            if (result != QMessageBox::Yes) {
                return;
            }
            context_.services().backups->restore(backupPath);
            statusBar()->showMessage(QStringLiteral("Backup restored. Restart native app to reload all screens."), 9000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Restore Backup"), err);
        }
    });
    layout->addWidget(panel);
    layout->addStretch(1);
    return page;
}

QWidget* DesktopApplication::buildInventoryPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 18, 24, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Inventory Management"), QStringLiteral("Monthly stock quantities, purchases, cost, and reorder levels."), page));
    layout->addWidget(createContextBar(context_, page));

    const QString financialYear = currentFinancialYearValue();
    QFrame* toolbar = createPanel(page);
    QVBoxLayout* toolbarOuter = new QVBoxLayout(toolbar);
    toolbarOuter->setContentsMargins(14, 12, 14, 12);
    toolbarOuter->setSpacing(10);
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);
    QComboBox* month = new QComboBox(toolbar);
    month->addItems({
        QStringLiteral("01 - January"), QStringLiteral("02 - February"), QStringLiteral("03 - March"),
        QStringLiteral("04 - April"), QStringLiteral("05 - May"), QStringLiteral("06 - June"),
        QStringLiteral("07 - July"), QStringLiteral("08 - August"), QStringLiteral("09 - September"),
        QStringLiteral("10 - October"), QStringLiteral("11 - November"), QStringLiteral("12 - December")
    });
    month->setCurrentIndex(QDate::currentDate().month() - 1);
    QLineEdit* product = new QLineEdit(toolbar);
    product->setPlaceholderText(QStringLiteral("Product name"));
    QPushButton* add = new QPushButton(QStringLiteral("Add Product"), toolbar);
    add->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), toolbar);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* preview = new QPushButton(QStringLiteral("Preview PDF"), toolbar);
    preview->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* mail = new QPushButton(QStringLiteral("Send PDF"), toolbar);
    mail->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* save = new QPushButton(QStringLiteral("Save Inventory"), toolbar);
    QLabel* periodChip = new QLabel(inventoryDateRangeText(financialYear, month->currentIndex() + 1), toolbar);
    QLabel* tableSummary = new QLabel(QStringLiteral("No inventory loaded"), toolbar);
    periodChip->setObjectName(QStringLiteral("contextChip"));
    tableSummary->setObjectName(QStringLiteral("pageMeta"));
    toolbarLayout->addWidget(new QLabel(QStringLiteral("Month"), toolbar));
    toolbarLayout->addWidget(month);
    toolbarLayout->addWidget(product, 1);
    toolbarLayout->addWidget(add);
    toolbarLayout->addWidget(refresh);
    toolbarLayout->addWidget(preview);
    toolbarLayout->addWidget(mail);
    toolbarLayout->addWidget(save);
    QHBoxLayout* toolbarMeta = new QHBoxLayout();
    toolbarMeta->setSpacing(10);
    toolbarMeta->addWidget(periodChip);
    toolbarMeta->addWidget(tableSummary, 1);
    toolbarOuter->addLayout(toolbarLayout);
    toolbarOuter->addLayout(toolbarMeta);
    layout->addWidget(toolbar);

    QTableWidget* table = createInventoryTable(page);
    loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
    layout->addWidget(table, 1);

    auto refreshSummary = [table, tableSummary, periodChip, financialYear, month]() {
        int productCount = 0;
        for (int rowIndex = 0; rowIndex < table->rowCount(); rowIndex += 1) {
            const QTableWidgetItem* item = table->item(rowIndex, 1);
            if (item && !item->text().trimmed().isEmpty() && !item->text().startsWith(QStringLiteral("No inventory rows yet"))) {
                productCount += 1;
            }
        }
        tableSummary->setText(QStringLiteral("%1 products | Edit quantities directly | Enter moves to next cell").arg(productCount));
        periodChip->setText(inventoryDateRangeText(financialYear, month->currentIndex() + 1));
    };
    refreshSummary();

    connect(month, &QComboBox::currentIndexChanged, this, [this, table, financialYear, month](int) {
        loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
    });
    connect(month, &QComboBox::currentIndexChanged, this, [refreshSummary](int) {
        refreshSummary();
    });
    connect(refresh, &QPushButton::clicked, this, [this, table, financialYear, month, refreshSummary]() {
        loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
        refreshSummary();
    });
    connect(add, &QPushButton::clicked, this, [table, product, refreshSummary]() {
        const QString name = product->text().trimmed();
        if (name.isEmpty()) {
            return;
        }
        const bool hasOnlyEmptyState = table->rowCount() == 1
            && table->item(0, 1)
            && table->item(0, 1)->text().startsWith(QStringLiteral("No inventory rows yet"));
        if (hasOnlyEmptyState) {
            table->clearSpans();
            table->setRowCount(0);
        }
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, createTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        table->setItem(row, 1, createTableItem(name, Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        table->setItem(row, 2, createNumberTableItem(QStringLiteral("0.00"), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        table->setItem(row, 3, createNumberTableItem(QStringLiteral("0.00"), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        for (int column = 4; column < table->columnCount(); column += 1) {
            table->setItem(row, column, createNumberTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        }
        product->clear();
        table->setCurrentCell(row, 1);
        table->editItem(table->item(row, 1));
        refreshSummary();
    });
    connect(product, &QLineEdit::returnPressed, add, &QPushButton::click);
    connect(save, &QPushButton::clicked, this, [this, table, financialYear, month, refreshSummary]() {
        try {
            context_.services().inventory->saveSnapshot(financialYear, month->currentIndex() + 1, inventoryRowsFromTable(*table));
            loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
            refreshSummary();
            statusBar()->showMessage(QStringLiteral("Inventory saved"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Save Inventory"), err);
        }
    });
    connect(preview, &QPushButton::clicked, this, [this, table, financialYear, month]() {
        try {
            const QJsonArray sourceRows = inventoryRowsFromTable(*table);
            QVector<domain::InventoryProductRow> rows;
            for (const QJsonValue& value : sourceRows) {
                const QJsonObject item = value.toObject();
                QVector<double> quantities;
                for (int day = 1; day <= 31; day += 1) {
                    quantities.append(item.value(QStringLiteral("qty_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')))).toDouble());
                }
                rows.append(domain::InventoryProductRow{
                    item.value(QStringLiteral("name")).toString(),
                    item.value(QStringLiteral("cost")).toDouble(),
                    item.value(QStringLiteral("min_stock")).toDouble(),
                    quantities
                });
            }
            const QByteArray pdf = context_.services().inventory->buildPdfPreview(domain::InventoryPdfMailRequest{
                QString(), QString(), QString(), QString(), QStringLiteral("smtp.gmail.com"), 587,
                QStringLiteral("Inventory Report"), QString(), financialYear, month->currentText(), QStringLiteral("monthly"), false, rows
            });
            const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Inventory PDF"), QStringLiteral("Inventory_%1_%2.pdf").arg(financialYear, QString::number(month->currentIndex() + 1)), QStringLiteral("PDF (*.pdf)"));
            if (path.trimmed().isEmpty()) {
                return;
            }
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                throw std::runtime_error(QStringLiteral("Could not write PDF: %1").arg(path).toStdString());
            }
            file.write(pdf);
            statusBar()->showMessage(QStringLiteral("Inventory PDF saved"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Inventory PDF"), err);
        }
    });
    connect(mail, &QPushButton::clicked, this, [this, table, financialYear, month]() {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Send Inventory PDF"));
        QGridLayout* form = new QGridLayout(&dialog);
        QLineEdit* to = new QLineEdit(&dialog);
        QLineEdit* cc = new QLineEdit(&dialog);
        QLineEdit* from = new QLineEdit(&dialog);
        QLineEdit* password = new QLineEdit(&dialog);
        QLineEdit* host = new QLineEdit(QStringLiteral("smtp.gmail.com"), &dialog);
        QLineEdit* port = new QLineEdit(QStringLiteral("587"), &dialog);
        QLineEdit* subject = new QLineEdit(QStringLiteral("Inventory Report"), &dialog);
        QLineEdit* notes = new QLineEdit(&dialog);
        password->setEchoMode(QLineEdit::Password);
        QPushButton* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
        cancel->setObjectName(QStringLiteral("secondaryButton"));
        QPushButton* send = new QPushButton(QStringLiteral("Send"), &dialog);
        form->addWidget(new QLabel(QStringLiteral("To"), &dialog), 0, 0);
        form->addWidget(to, 0, 1);
        form->addWidget(new QLabel(QStringLiteral("CC"), &dialog), 1, 0);
        form->addWidget(cc, 1, 1);
        form->addWidget(new QLabel(QStringLiteral("From"), &dialog), 2, 0);
        form->addWidget(from, 2, 1);
        form->addWidget(new QLabel(QStringLiteral("Password"), &dialog), 3, 0);
        form->addWidget(password, 3, 1);
        form->addWidget(new QLabel(QStringLiteral("SMTP Host"), &dialog), 4, 0);
        form->addWidget(host, 4, 1);
        form->addWidget(new QLabel(QStringLiteral("Port"), &dialog), 5, 0);
        form->addWidget(port, 5, 1);
        form->addWidget(new QLabel(QStringLiteral("Subject"), &dialog), 6, 0);
        form->addWidget(subject, 6, 1);
        form->addWidget(new QLabel(QStringLiteral("Notes"), &dialog), 7, 0);
        form->addWidget(notes, 7, 1);
        form->addWidget(cancel, 8, 0);
        form->addWidget(send, 8, 1);
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(send, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        try {
            const QJsonArray sourceRows = inventoryRowsFromTable(*table);
            QVector<domain::InventoryProductRow> rows;
            for (const QJsonValue& value : sourceRows) {
                const QJsonObject item = value.toObject();
                QVector<double> quantities;
                for (int day = 1; day <= 31; day += 1) {
                    quantities.append(item.value(QStringLiteral("qty_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')))).toDouble());
                }
                rows.append(domain::InventoryProductRow{
                    item.value(QStringLiteral("name")).toString(),
                    item.value(QStringLiteral("cost")).toDouble(),
                    item.value(QStringLiteral("min_stock")).toDouble(),
                    quantities
                });
            }
            context_.services().inventory->sendPdfMail(domain::InventoryPdfMailRequest{
                to->text(), cc->text(), from->text(), password->text(), host->text(), port->text().toInt(),
                subject->text(), notes->text(), financialYear, month->currentText(), QStringLiteral("monthly"), false, rows
            });
            statusBar()->showMessage(QStringLiteral("Inventory PDF sent"), 9000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Send Inventory PDF"), err);
        }
    });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+S")), [save]() { save->click(); });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+K")), [product]() { focusEntryWidget(*product); });
    return page;
}

QWidget* DesktopApplication::buildInventoryValuePage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 22);
    layout->setSpacing(12);
    layout->addWidget(createPageHeader(QStringLiteral("Stock Value Report"), QStringLiteral("Daily stock quantity and value from saved inventory snapshots."), page));
    layout->addWidget(createContextBar(context_, page));

    const QString financialYear = currentFinancialYearValue();
    QFrame* toolbar = createPanel(page);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    QComboBox* month = new QComboBox(toolbar);
    month->addItems({
        QStringLiteral("01 - January"), QStringLiteral("02 - February"), QStringLiteral("03 - March"),
        QStringLiteral("04 - April"), QStringLiteral("05 - May"), QStringLiteral("06 - June"),
        QStringLiteral("07 - July"), QStringLiteral("08 - August"), QStringLiteral("09 - September"),
        QStringLiteral("10 - October"), QStringLiteral("11 - November"), QStringLiteral("12 - December")
    });
    month->setCurrentIndex(QDate::currentDate().month() - 1);
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), toolbar);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    toolbarLayout->addWidget(new QLabel(QStringLiteral("Month"), toolbar));
    toolbarLayout->addWidget(month);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(refresh);
    layout->addWidget(toolbar);

    QTableWidget* table = createDataTable(page);
    loadInventoryValue(*table, financialYear, month->currentIndex() + 1);
    layout->addWidget(table, 1);
    connect(month, &QComboBox::currentIndexChanged, this, [this, table, financialYear, month](int) {
        loadInventoryValue(*table, financialYear, month->currentIndex() + 1);
    });
    connect(refresh, &QPushButton::clicked, this, [this, table, financialYear, month]() {
        loadInventoryValue(*table, financialYear, month->currentIndex() + 1);
    });
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

void DesktopApplication::loadInventorySnapshot(QTableWidget& table, const QString& financialYear, int month) {
    try {
        const int visibleDays = daysInInventoryMonth(month);
        QStringList headers = {QStringLiteral("row_id"), QStringLiteral("Product"), QStringLiteral("Cost"), QStringLiteral("Min Stock")};
        for (int day = 1; day <= 31; day += 1) {
            headers.append(QStringLiteral("%1 Qty").arg(QString::number(day).rightJustified(2, QLatin1Char('0'))));
        }
        for (int day = 1; day <= 31; day += 1) {
            headers.append(QStringLiteral("%1 In").arg(QString::number(day).rightJustified(2, QLatin1Char('0'))));
        }

        const QJsonArray rows = context_.services().inventory->loadSnapshot(financialYear, month);
        const int rowCount = rows.isEmpty() ? 1 : rows.size();
        table.clearSpans();
        table.clear();
        table.setColumnCount(headers.size());
        table.setRowCount(rowCount);
        table.setHorizontalHeaderLabels(headers);
        table.setColumnHidden(0, true);

        table.setColumnWidth(1, 230);
        table.setColumnWidth(2, 95);
        table.setColumnWidth(3, 100);
        for (int day = 1; day <= 31; day += 1) {
            const bool visible = day <= visibleDays;
            table.setColumnHidden(3 + day, !visible);
            table.setColumnHidden(34 + day, !visible);
            table.setColumnWidth(3 + day, 78);
            table.setColumnWidth(34 + day, 72);
        }

        if (rows.isEmpty()) {
            QTableWidgetItem* empty = createTableItem(QStringLiteral("No inventory rows yet. Type a product name above and press Enter."), Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            table.setItem(0, 1, empty);
            for (int columnIndex = 2; columnIndex < headers.size(); columnIndex += 1) {
                table.setItem(0, columnIndex, createTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            }
            table.setSpan(0, 1, 1, 6);
            return;
        }

        for (int rowIndex = 0; rowIndex < rows.size(); rowIndex += 1) {
            const QJsonObject row = rows.at(rowIndex).toObject();
            table.setItem(rowIndex, 0, createTableItem(row.value(QStringLiteral("row_id")).toVariant().toString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            table.setItem(rowIndex, 1, createTableItem(row.value(QStringLiteral("name")).toString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
            table.setItem(rowIndex, 2, createNumberTableItem(moneyText(row.value(QStringLiteral("cost")).toDouble()), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
            table.setItem(rowIndex, 3, createNumberTableItem(moneyText(row.value(QStringLiteral("min_stock")).toDouble()), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
            for (int day = 1; day <= 31; day += 1) {
                const QString qtyKey = QStringLiteral("qty_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')));
                const QString purchaseKey = QStringLiteral("purchase_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')));
                const double qty = row.value(qtyKey).toDouble();
                const double purchase = row.value(purchaseKey).toDouble();
                table.setItem(rowIndex, 3 + day, createNumberTableItem(qty == 0.0 ? QString() : moneyText(qty), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
                table.setItem(rowIndex, 34 + day, createNumberTableItem(purchase == 0.0 ? QString() : moneyText(purchase), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
            }
        }
    } catch (const std::exception& err) {
        showError(QStringLiteral("Inventory"), err);
    }
}

void DesktopApplication::loadInventoryValue(QTableWidget& table, const QString& financialYear, int month) {
    try {
        setTableRows(table, {QStringLiteral("day"), QStringLiteral("quantity"), QStringLiteral("stock_value")}, context_.services().inventory->stockValue(financialYear, month));
    } catch (const std::exception& err) {
        showError(QStringLiteral("Stock Value"), err);
    }
}

void DesktopApplication::loadAuditLogs(QTableWidget& table) {
    try {
        setTableRows(table, {QStringLiteral("timestamp"), QStringLiteral("username"), QStringLiteral("action"), QStringLiteral("details")}, context_.services().audit->listAuditLogs(QStringLiteral("native")));
    } catch (const std::exception& err) {
        showError(QStringLiteral("Audit Logs"), err);
    }
}

void DesktopApplication::loadReportTable(QTableWidget& table, const QString& reportName, const QString& partyName, const domain::ReportRange& range) {
    try {
        if (range.start > range.end) {
            throw std::invalid_argument("Report start date must be before end date");
        }
        if (reportName == QStringLiteral("Party Ledger")) {
            if (partyName.trimmed().isEmpty()) {
                QJsonArray rows;
                QJsonObject placeholder;
                placeholder.insert(QStringLiteral("date"), QStringLiteral("--"));
                placeholder.insert(QStringLiteral("bill_no"), QStringLiteral("--"));
                placeholder.insert(QStringLiteral("type"), QStringLiteral("Enter a party name and refresh"));
                placeholder.insert(QStringLiteral("mode"), QStringLiteral("--"));
                placeholder.insert(QStringLiteral("amount"), 0.0);
                placeholder.insert(QStringLiteral("balance"), 0.0);
                rows.append(placeholder);
                setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount"), QStringLiteral("balance")}, rows);
                return;
            }
            const QJsonObject report = context_.services().reports->ledger(partyName, range);
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount"), QStringLiteral("balance")}, report.value(QStringLiteral("data")).toArray());
            return;
        }
        if (reportName == QStringLiteral("Day Book")) {
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")}, context_.services().reports->dayBook(range.end));
            return;
        }
        if (reportName == QStringLiteral("Purchase Report") || reportName == QStringLiteral("Expenses")) {
            const QString targetType = reportName == QStringLiteral("Purchase Report") ? QStringLiteral("Purchase") : QStringLiteral("Expense");
            const int days = static_cast<int>(std::max<qint64>(1, range.start.daysTo(QDate::currentDate()) + 1));
            const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 1000, days);
            QJsonArray data;
            for (const domain::TransactionRow& row : rows) {
                if (row.date < range.start || row.date > range.end) {
                    continue;
                }
                if (transactionTypeText(row.type) != targetType) {
                    continue;
                }
                data.append(transactionToJson(row));
            }
            setTableRows(table, {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")}, data);
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

void DesktopApplication::applyTableSearch(QTableWidget& table, const QString& query) {
    const QString text = query.trimmed();
    for (int rowIndex = 0; rowIndex < table.rowCount(); rowIndex += 1) {
        table.setRowHidden(rowIndex, !text.isEmpty() && !rowContainsText(table, rowIndex, text));
    }
}

QStringList DesktopApplication::partyNames() {
    QStringList names;
    const QJsonArray rows = context_.services().parties->listParties();
    for (const QJsonValue& value : rows) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            names.append(name);
        }
    }
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

QJsonArray DesktopApplication::inventoryRowsFromTable(QTableWidget& table) {
    QJsonArray rows;
    for (int rowIndex = 0; rowIndex < table.rowCount(); rowIndex += 1) {
        const QTableWidgetItem* nameItem = table.item(rowIndex, 1);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty() || name.startsWith(QStringLiteral("No inventory rows yet"))) {
            continue;
        }

        QJsonObject row;
        const QTableWidgetItem* rowIdItem = table.item(rowIndex, 0);
        row.insert(QStringLiteral("row_id"), rowIdItem ? rowIdItem->text().trimmed() : QString());
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("cost"), table.item(rowIndex, 2) ? table.item(rowIndex, 2)->text().toDouble() : 0.0);
        row.insert(QStringLiteral("min_stock"), table.item(rowIndex, 3) ? table.item(rowIndex, 3)->text().toDouble() : 0.0);
        for (int day = 1; day <= 31; day += 1) {
            const int qtyColumn = 3 + day;
            const int purchaseColumn = 34 + day;
            const QString qtyKey = QStringLiteral("qty_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')));
            const QString purchaseKey = QStringLiteral("purchase_%1").arg(QString::number(day).rightJustified(2, QLatin1Char('0')));
            row.insert(qtyKey, table.item(rowIndex, qtyColumn) ? table.item(rowIndex, qtyColumn)->text().toDouble() : 0.0);
            row.insert(purchaseKey, table.item(rowIndex, purchaseColumn) ? table.item(rowIndex, purchaseColumn)->text().toDouble() : 0.0);
        }
        rows.append(row);
    }
    return rows;
}

void DesktopApplication::setTableRows(QTableWidget& table, const QStringList& headers, const QJsonArray& rows) {
    table.clear();
    table.setColumnCount(headers.size());
    table.setHorizontalHeaderLabels(headers);
    table.clearSpans();
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table.horizontalHeader()->setStretchLastSection(true);

    if (rows.isEmpty()) {
        table.setRowCount(1);
        QTableWidgetItem* empty = createTableItem(QStringLiteral("No data"), Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table.setItem(0, 0, empty);
        for (int columnIndex = 1; columnIndex < headers.size(); columnIndex += 1) {
            table.setItem(0, columnIndex, createTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        }
        if (headers.size() > 1) {
            table.setSpan(0, 0, 1, headers.size());
        }
        return;
    }

    table.setRowCount(rows.size());

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
            QTableWidgetItem* item = value.isDouble()
                ? createNumberTableItem(text, Qt::ItemIsEnabled | Qt::ItemIsSelectable)
                : createTableItem(text, Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (columnIndex == 0 && row.contains(QStringLiteral("id"))) {
                item->setData(Qt::UserRole, row.value(QStringLiteral("id")).toInt());
            }
            table.setItem(rowIndex, columnIndex, item);
        }
    }
    table.resizeColumnsToContents();
    table.horizontalHeader()->setStretchLastSection(true);
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
            statusBar()->showMessage(QStringLiteral("Native updates are managed by GitHub release artifacts"), 7000);
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
