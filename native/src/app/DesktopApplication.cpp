#include "app/DesktopApplication.h"

#include "app/SplashScreen.h"
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
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QToolBar>
#include <QVariant>
#include <QVBoxLayout>
#include <QTimer>
#include <QVariantAnimation>

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
    layout->setContentsMargins(0, 4, 0, 4);
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
    table->verticalHeader()->setDefaultSectionSize(26);
    table->horizontalHeader()->setMinimumHeight(30);
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
    table->horizontalHeader()->setMinimumSectionSize(60);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->installEventFilter(new InventoryTableKeyFilter(*table));
    return table;
}

QListWidgetItem* addGroupItem(QListWidget& nav, const QString& label) {
    QListWidgetItem* item = new QListWidgetItem(label.toUpper(), &nav);
    item->setFlags(Qt::NoItemFlags);
    item->setData(Qt::UserRole, -1);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    QFont groupFont;
    groupFont.setPixelSize(10);
    groupFont.setWeight(QFont::Bold);
    item->setFont(groupFont);
    item->setSizeHint(QSize(0, 32));
    return item;
}

QListWidgetItem* addNavItem(QListWidget& nav, const QString& label, int pageIndex) {
    QListWidgetItem* item = new QListWidgetItem(label, &nav);
    item->setData(Qt::UserRole, pageIndex);
    return item;
}

QListWidgetItem* addNavItem(QListWidget& nav, const QString& label, int pageIndex, QStyle::StandardPixmap icon) {
    QListWidgetItem* item = addNavItem(nav, label, pageIndex);
    item->setIcon(qApp->style()->standardIcon(icon));
    return item;
}

QFrame* createMetricTile(const QString& label, const QString& value, QWidget* parent) {
    QFrame* tile = createPanel(parent);
    QVBoxLayout* layout = new QVBoxLayout(tile);
    layout->setContentsMargins(16, 14, 16, 14);
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
    // Use a fixed non-leap year (2023) for consistent day counts across all financial years.
    // April–December map to 2023; January–March map to 2024 (next calendar year in FY).
    const int calendarYear = month >= 4 ? 2023 : 2024;
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

void animatePanel(QWidget& widget, int delayMs, int durationMs = 500) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(&widget);
    effect->setOpacity(0.0);
    widget.setGraphicsEffect(effect);
    QPropertyAnimation* fade = new QPropertyAnimation(effect, "opacity", &widget);
    fade->setDuration(durationMs);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(delayMs, fade, [fade]() { fade->start(QAbstractAnimation::DeleteWhenStopped); });
}

QString moneyText(double value) {
    return QStringLiteral("$%1").arg(value, 0, 'f', 2);
}

QString humanizeHeaderKey(const QString& key) {
    if (key == QStringLiteral("id")) {
        return QStringLiteral("ID");
    }
    if (key == QStringLiteral("bill_no")) {
        return QStringLiteral("Bill No");
    }
    if (key == QStringLiteral("min_stock")) {
        return QStringLiteral("Min Stock");
    }
    if (key == QStringLiteral("stock_value")) {
        return QStringLiteral("Stock Value");
    }
    if (key == QStringLiteral("sale_returns")) {
        return QStringLiteral("Sale Returns");
    }
    QStringList parts = key.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    for (QString& part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
        }
    }
    return parts.join(QStringLiteral(" "));
}

QStringList humanizeHeaderKeys(const QStringList& headers) {
    QStringList labels;
    labels.reserve(headers.size());
    for (const QString& key : headers) {
        labels.append(humanizeHeaderKey(key));
    }
    return labels;
}

void ensureTableHeaders(QTableWidget& table, const QStringList& headers) {
    table.clear();
    table.clearSpans();
    table.setRowCount(0);
    table.setColumnCount(headers.size());
    table.setHorizontalHeaderLabels(humanizeHeaderKeys(headers));
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

static QString buildModernQss(bool darkMode = false) {
    // Mac-grade minimalist design — Tailwind palette, flat, borderless
    const QString bg           = darkMode ? QStringLiteral("#111827") : QStringLiteral("#eeeeee");
    const QString surface      = darkMode ? QStringLiteral("#1f2937") : QStringLiteral("#ffffff");
    const QString sidebarBg    = darkMode ? QStringLiteral("#1f2937") : QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #d9f2f6, stop:0.55 #cfe4df, stop:1 #ead8bd)");
    const QString sidebarSel   = darkMode ? QStringLiteral("#3b82f6") : QStringLiteral("rgba(105, 126, 142, 0.20)");
    const QString sidebarSelTx = darkMode ? QStringLiteral("#ffffff") : QStringLiteral("#0c0c0c");
    const QString sidebarHover = darkMode ? QStringLiteral("#374151") : QStringLiteral("rgba(255, 255, 255, 0.35)");
    const QString textPrimary  = darkMode ? QStringLiteral("#f9fafb") : QStringLiteral("#0c0c0c");
    const QString textSecondary= darkMode ? QStringLiteral("#9ca3af") : QStringLiteral("#777777");
    const QString border       = darkMode ? QStringLiteral("#374151") : QStringLiteral("#d9d9d9");
    const QString inputBorder  = darkMode ? QStringLiteral("#4b5563") : QStringLiteral("#d7d7d7");
    const QString tableHeaderBg= darkMode ? QStringLiteral("#1f2937") : QStringLiteral("#ffffff");
    const QString tableHeaderTx= darkMode ? QStringLiteral("#9ca3af") : QStringLiteral("#111111");
    const QString tableAltRow  = darkMode ? QStringLiteral("#1a2230") : QStringLiteral("#f1f1f1");
    const QString tableGrid    = darkMode ? QStringLiteral("#374151") : QStringLiteral("#e8e8e8");
    const QString tableSelBg   = darkMode ? QStringLiteral("#1e3a5f") : QStringLiteral("#dfe9ef");
    const QString accent       = darkMode ? QStringLiteral("#3b82f6") : QStringLiteral("#147de8");
    const QString accentHover  = darkMode ? QStringLiteral("#2563eb") : QStringLiteral("#0f6fd0");
    const QString accentPanelBg= darkMode ? QStringLiteral("#1e3a5f") : QStringLiteral("#eef6ff");
    const QString inputBg      = darkMode ? QStringLiteral("#1f2937") : QStringLiteral("#ffffff");
    const QString scrollHandle = darkMode ? QStringLiteral("#4b5563") : QStringLiteral("#c7c7c7");
    const QString secondaryBg  = darkMode ? QStringLiteral("#374151") : QStringLiteral("#e8e8e8");
    const QString secondaryHover = darkMode ? QStringLiteral("#4b5563") : QStringLiteral("#dcdcdc");
    const QString heroGrad     = darkMode
        ? QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2563eb, stop:1 #60a5fa)")
        : QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #3b82f6, stop:1 #60a5fa)");

    return QStringLiteral(
        // 1. Universal font reset — normal weight baseline
        "* { font-family: 'Segoe UI', '-apple-system', 'Helvetica Neue', sans-serif;"
        " font-weight: 400; }"

        // 2. Workspace
        "QMainWindow, QWidget#workspace { background: %1; color: %2; font-size: 13px; }"
    ).arg(bg, textPrimary)

    // 3. Sidebar container
    + QStringLiteral(
        "QWidget#sidebarWrap { background: %1; border-right: 1px solid rgba(0,0,0,0.14); }"
    ).arg(sidebarBg, border)

    // 4. Sidebar list
    + QStringLiteral(
        "QListWidget#sidebar { background: transparent; border: none; color: %1;"
        " font-size: 13px; font-weight: 400; outline: none; }"
    ).arg(textPrimary)

    // 5. Sidebar items
    + QStringLiteral(
        "QListWidget#sidebar::item { padding: 5px 10px; border-radius: 4px; min-height: 20px; }"
    )

    // 6. Sidebar selected — vibrant blue with white bold text
    + QStringLiteral(
        "QListWidget#sidebar::item:selected { background: %1; color: %2; font-weight: 400; }"
    ).arg(sidebarSel, sidebarSelTx)

    // 7. Sidebar hover
    + QStringLiteral(
        "QListWidget#sidebar::item:hover:!selected { background: %1; }"
    ).arg(sidebarHover)

    // 8. Brand label
    + QStringLiteral(
        "QLabel#brandLabel { color: %1; font-size: 13px; font-weight: 400;"
        " padding: 11px 14px 10px 14px; }"
    ).arg(textPrimary)

    // 9. Brand sub
    + QStringLiteral(
        "QLabel#brandSub { color: %1; font-size: 11px; padding: 0 14px 10px 14px; }"
    ).arg(textSecondary)

    // 10. Page title
    + QStringLiteral(
        "QLabel#pageTitle { color: %1; font-size: 28px; font-weight: 700; letter-spacing: 0px; }"
    ).arg(textPrimary)

    // 11. Page meta
    + QStringLiteral(
        "QLabel#pageMeta { color: %1; font-size: 12px; }"
    ).arg(textSecondary)

    // 12. Section title
    + QStringLiteral(
        "QLabel#sectionTitle { color: %1; font-size: 14px; font-weight: 600; }"
    ).arg(textPrimary)

    + QStringLiteral(
        "QLabel#sectionDescription { color: %1; font-size: 12px; }"
    ).arg(textSecondary)

    // 13. Metric label
    + QStringLiteral(
        "QLabel#metricLabel { color: %1; font-size: 11px; font-weight: 500; }"
    ).arg(textSecondary)

    // 14. Metric value
    + QStringLiteral(
        "QLabel#metricValue { color: %1; font-size: 20px; font-weight: 600; }"
    ).arg(textPrimary)

    + QStringLiteral(
        "QLabel#contextChip { color: %1; font-size: 12px; }"
    ).arg(textSecondary)

    // 15. Panel — no border, just white space
    + QStringLiteral(
        "QFrame#panel { background: %1; border: 1px solid %2; border-radius: 8px; }"
    ).arg(surface, border)

    // 15b. Form panel — visible border
    + QStringLiteral(
        "QFrame#formPanel { background: %1; border: 1px solid %2; border-radius: 8px; }"
    ).arg(surface, border)

    // 16. Accent panel
    + QStringLiteral(
        "QFrame#accentPanel { background: %1; border: none; border-radius: 6px; }"
    ).arg(accentPanelBg)

    // 17. Context bar — transparent, no border
    + QStringLiteral(
        "QFrame#contextBar { background: transparent; border: none; padding: 0; }"
    )

    // 18. Tables — flat, clean, soft-blue selection, no vertical grid
    + QStringLiteral(
        "QTableWidget#dataTable, QTableWidget#inventoryTable {"
        " background: %1; alternate-background-color: %2;"
        " border: none; border-radius: 0px;"
        " selection-background-color: %3; selection-color: %4;"
        " gridline-color: %5; color: %4; font-size: 12px; font-weight: 400;"
        " outline: none; }"
    ).arg(surface, tableAltRow, tableSelBg, textPrimary, tableGrid)

    // 19. Header — flat white, small uppercase muted text, bottom border only
    + QStringLiteral(
        "QHeaderView::section {"
        " background-color: %1; color: %2;"
        " border: none; border-bottom: 1px solid %3;"
        " padding: 5px 8px; font-weight: 700; font-size: 11px;"
        " letter-spacing: 0px; }"
    ).arg(tableHeaderBg, tableHeaderTx, border)

    // 20. Header background fill
    + QStringLiteral(
        "QHeaderView { background-color: %1; }"
    ).arg(tableHeaderBg)

    // 21. Table items — tight padding, thin bottom border
    + QStringLiteral(
        "QTableWidget::item { padding: 3px 8px; border-bottom: 1px solid %1; color: %2; }"
        "QTableWidget#inventoryTable::item { padding: 4px 6px; border-right: 1px solid %1; }"
    ).arg(tableGrid, textPrimary)

    // 22. Primary button — solid blue, white bold text
    + QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; border: none;"
        " border-radius: 6px; min-height: 24px; padding: 2px 16px;"
        " font-weight: 600; font-size: 12px; }"
    ).arg(accent)

    // 23. Button hover
    + QStringLiteral(
        "QPushButton:hover { background: %1; }"
        "QPushButton:pressed { background: %1; }"
    ).arg(accentHover)

    // 24. Secondary button — grey fill, no border (Clear / Delete)
    + QStringLiteral(
        "QPushButton#secondaryButton { background: %1; color: %2;"
        " border: none; }"
        "QPushButton#secondaryButton:hover { background: %3; }"
    ).arg(secondaryBg, textPrimary, secondaryHover)

    // 25. Inputs — 32px height, light border, 5px radius
    + QStringLiteral(
        "QLineEdit, QDateEdit, QDoubleSpinBox, QComboBox {"
        " background: %1; border: 1px solid %2; border-radius: 5px;"
        " min-height: 23px; padding: 0 8px; color: %3; font-size: 12px; }"
    ).arg(inputBg, inputBorder, textPrimary)

    // 26. Focus states
    + QStringLiteral(
        "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QComboBox:focus {"
        " border-color: %1; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
    ).arg(accent)

    // 27. Scrollbars — 8px, subtle
    + QStringLiteral(
        "QScrollBar:vertical { background: transparent; width: 8px; border-radius: 4px; margin: 0; }"
        "QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: transparent; height: 8px; border-radius: 4px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: %1; border-radius: 4px; min-width: 20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    ).arg(scrollHandle)

    // 28. Status bar
    + QStringLiteral(
        "QStatusBar { background: %1; color: %2; border-top: 1px solid %3;"
        " font-size: 12px; }"
    ).arg(surface, textSecondary, border)

    // 29. Dialogs
    + QStringLiteral(
        "QDialog { background: %1; }"
        "QMessageBox { background: %1; }"
    ).arg(bg)

    // Toolbar
    + QStringLiteral(
        "QToolBar { background: %1; border-bottom: 1px solid %2; spacing: 8px; padding: 0 12px; min-height: 38px; }"
        "QToolBar QLabel { color: %3; font-size: 12px; }"
        "QLabel#toolbarTitle { color: %3; font-size: 13px; font-weight: 400; }"
        "QToolButton { background: transparent; color: %3; border: none; padding: 4px 2px; font-size: 12px; }"
        "QToolButton:hover { background: rgba(0,0,0,0.04); border-radius: 4px; }"
    ).arg(surface, border, textPrimary)

    // 30. Welcome hero — accent blue gradient
    + QStringLiteral(
        "QFrame#welcomeHero {"
        " background: %1;"
        " border-radius: 8px; border: none; }"
    ).arg(heroGrad)

    + QStringLiteral(
        "QLabel#welcomeKicker {"
        " color: rgba(255,255,255,0.7); font-size: 10px; font-weight: 600; letter-spacing: 2px; }"

        "QLabel#welcomeTitle {"
        " color: #ffffff; font-size: 28px; font-weight: 700; }"

        "QLabel#welcomeSubtitle {"
        " color: rgba(255,255,255,0.8); font-size: 13px; }"
    )

    // 31. Welcome stat chips — subtle bg
    + QStringLiteral(
        "QLabel#welcomeStat {"
        " color: #ffffff; background: rgba(255,255,255,0.15);"
        " border: 1px solid rgba(255,255,255,0.2);"
        " border-radius: 12px; padding: 4px 12px;"
        " font-size: 11px; font-weight: 500; }"
    )

    // 32. Feature cards — clean, no border
    + QStringLiteral(
        "QFrame#featureCard {"
        " background: %1; border: none; border-radius: 6px; }"
        "QFrame#featureCard:hover { background: %2; }"
    ).arg(surface, sidebarHover)

    + QStringLiteral(
        "QLabel#featureIcon { color: %1; font-size: 22px; }"
        "QLabel#featureTitle { color: %2; font-size: 13px; font-weight: 600; }"
        "QLabel#featureDesc { color: %3; font-size: 12px; }"
    ).arg(accent, textPrimary, textSecondary)

    // 33. Onboarding steps — clean, minimal
    + QStringLiteral(
        "QFrame#onboardStep { background: %1; border: none; border-radius: 6px; }"
    ).arg(surface)

    + QStringLiteral(
        "QLabel#stepBadge {"
        " background: %1; color: #ffffff; border-radius: 11px;"
        " font-size: 11px; font-weight: 600;"
        " min-width: 22px; max-width: 22px; min-height: 22px; max-height: 22px;"
        " qproperty-alignment: AlignCenter; }"
    ).arg(accent)

    + QStringLiteral(
        "QLabel#stepTitle { color: %1; font-size: 13px; font-weight: 600; }"
        "QLabel#stepDesc { color: %2; font-size: 12px; }"
    ).arg(textPrimary, textSecondary)

    + QStringLiteral(
        "QFrame#divider { background: %1; border: none; max-height: 1px; }"
    ).arg(border)

    + QStringLiteral(
        "QPushButton#heroCta {"
        " background: #ffffff; color: %1; border: none; border-radius: 6px;"
        " min-height: 28px; padding: 4px 18px; font-weight: 500; font-size: 13px; }"
        "QPushButton#heroCta:hover { background: #f0f0f0; }"
        "QPushButton#heroCta:pressed { background: #e0e0e0; }"
    ).arg(accent)

    + QStringLiteral(
        "QPushButton#heroGhost {"
        " background: transparent; color: #ffffff;"
        " border: 1px solid rgba(255,255,255,0.25); border-radius: 6px;"
        " min-height: 28px; padding: 4px 18px; font-weight: 500; font-size: 13px; }"
        "QPushButton#heroGhost:hover { background: rgba(255,255,255,0.18); }"
        "QPushButton#heroGhost:pressed { background: rgba(255,255,255,0.25); }"
    );
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

void DesktopApplication::applyTheme(bool darkMode) {
    const QString fontDir = QCoreApplication::applicationDirPath() + QStringLiteral("/fonts/");
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("Inter-Regular.ttf"));
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("Inter-Bold.ttf"));
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Regular.ttf"));
    QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Bold.ttf"));
    qApp->setFont(QFont(QStringLiteral("Segoe UI"), 13));
    qApp->setStyleSheet(buildModernQss(darkMode));
}

void DesktopApplication::buildNavigation() {
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("workspace"));
    QHBoxLayout* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QWidget* sidebar = new QWidget(root);
    sidebar->setObjectName(QStringLiteral("sidebarWrap"));
    sidebar->setFixedWidth(240);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    QWidget* sidebarTopInset = new QWidget(sidebar);
    sidebarTopInset->setFixedHeight(44);
    sidebarLayout->addWidget(sidebarTopInset);

    QListWidget* nav = new QListWidget(sidebar);
    nav->setObjectName(QStringLiteral("sidebar"));

    QStackedWidget* pages = new QStackedWidget(root);
    int pageIndex = 0;
    pages->addWidget(buildWelcomePage(*pages, *nav));
    addNavItem(*nav, QStringLiteral("Welcome"), pageIndex, QStyle::SP_DirHomeIcon);
    pageIndex += 1;
    pages->addWidget(buildDailyEntryPage());
    addNavItem(*nav, QStringLiteral("Daily Entry"), pageIndex, QStyle::SP_FileDialogDetailedView);
    pageIndex += 1;
    pages->addWidget(buildDashboardPage());
    addNavItem(*nav, QStringLiteral("Dashboard"), pageIndex, QStyle::SP_ComputerIcon);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Reports"), QStringLiteral("Report overview and transaction analysis.")));
    addNavItem(*nav, QStringLiteral("Reports"), pageIndex, QStyle::SP_FileDialogListView);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Party Ledger"), QStringLiteral("Party ledger with date filters and running balances.")));
    addNavItem(*nav, QStringLiteral("Party Ledger"), pageIndex, QStyle::SP_FileIcon);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Day Book"), QStringLiteral("Daily transaction register for selected dates.")));
    addNavItem(*nav, QStringLiteral("Day Book"), pageIndex, QStyle::SP_FileIcon);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Daily Summary"), QStringLiteral("Daily sales, returns, receipts, and expenses.")));
    addNavItem(*nav, QStringLiteral("Daily Summary"), pageIndex, QStyle::SP_FileDialogContentsView);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Short / Excess"), QStringLiteral("Cash-in-hand snapshots by day.")));
    addNavItem(*nav, QStringLiteral("Short / Excess"), pageIndex, QStyle::SP_DriveFDIcon);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Purchase Report"), QStringLiteral("Purchase summary and supplier analysis.")));
    addNavItem(*nav, QStringLiteral("Purchase Report"), pageIndex, QStyle::SP_FileDialogContentsView);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Expenses"), QStringLiteral("Expense transactions and totals.")));
    addNavItem(*nav, QStringLiteral("Expenses"), pageIndex, QStyle::SP_DirIcon);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Outstanding"), QStringLiteral("Customer balances for the selected financial year.")));
    addNavItem(*nav, QStringLiteral("Outstanding"), pageIndex, QStyle::SP_DialogHelpButton);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Trial Balance"), QStringLiteral("Account debit and credit balances.")));
    addNavItem(*nav, QStringLiteral("Trial Balance"), pageIndex, QStyle::SP_ArrowUp);
    pageIndex += 1;
    pages->addWidget(buildReportPage(QStringLiteral("Profit & Loss"), QStringLiteral("Sales, expenses, and net profit summary.")));
    addNavItem(*nav, QStringLiteral("P & L"), pageIndex, QStyle::SP_ArrowUp);
    pageIndex += 1;
    pages->addWidget(buildInventoryPage());
    addNavItem(*nav, QStringLiteral("Inventory"), pageIndex, QStyle::SP_DirIcon);
    pageIndex += 1;
    pages->addWidget(buildInventoryPage());
    addNavItem(*nav, QStringLiteral("Inventory Management"), pageIndex, QStyle::SP_FileDialogDetailedView);
    pageIndex += 1;
    pages->addWidget(buildInventoryValuePage());
    addNavItem(*nav, QStringLiteral("Stock Value Report"), pageIndex, QStyle::SP_FileDialogListView);
    pageIndex += 1;
    pages->addWidget(buildPartiesPage());
    addNavItem(*nav, QStringLiteral("Add Party"), pageIndex, QStyle::SP_FileDialogNewFolder);
    pageIndex += 1;
    pages->addWidget(buildAuditPage());
    addNavItem(*nav, QStringLiteral("Audit Logs"), pageIndex, QStyle::SP_MessageBoxInformation);
    pageIndex += 1;
    pages->addWidget(buildSettingsPage());
    addNavItem(*nav, QStringLiteral("Settings"), pageIndex, QStyle::SP_FileDialogInfoView);
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
    layout->setContentsMargins(20, 14, 20, 20);
    layout->setSpacing(14);

    // ── Hero panel ────────────────────────────────────────────────────────────
    QFrame* hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("welcomeHero"));
    hero->setMinimumHeight(220);
    QVBoxLayout* heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(28, 24, 28, 24);
    heroLayout->setSpacing(0);

    // Kicker
    QLabel* kicker = new QLabel(QStringLiteral("M-FINLOGS  ·  NATIVE ACCOUNTING WORKSPACE"), hero);
    kicker->setObjectName(QStringLiteral("welcomeKicker"));

    // Headline
    QLabel* headline = new QLabel(QStringLiteral("Welcome back."), hero);
    headline->setObjectName(QStringLiteral("welcomeTitle"));

    // Sub-headline
    QLabel* subline = new QLabel(
        currentCompanyText(context_) + QStringLiteral("   ·   ") + currentFinancialYearText(),
        hero
    );
    subline->setObjectName(QStringLiteral("welcomeSubtitle"));

    // Stat chips row
    QWidget* chipsRow = new QWidget(hero);
    QHBoxLayout* chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 18, 0, 0);
    chipsLayout->setSpacing(10);
    const QStringList chips = {
        QStringLiteral("⚡  Keyboard-first entry"),
        QStringLiteral("🗄  SQL Server backed"),
        QStringLiteral("📦  Native runtime"),
        QStringLiteral("📊  Live reports")
    };
    for (const QString& chip : chips) {
        QLabel* c = new QLabel(chip, chipsRow);
        c->setObjectName(QStringLiteral("welcomeStat"));
        chipsLayout->addWidget(c);
    }
    chipsLayout->addStretch(1);

    // CTA buttons row
    QWidget* ctaRow = new QWidget(hero);
    QHBoxLayout* ctaLayout = new QHBoxLayout(ctaRow);
    ctaLayout->setContentsMargins(0, 22, 0, 0);
    ctaLayout->setSpacing(12);
    QPushButton* ctaEntry    = new QPushButton(QStringLiteral("Open Daily Entry"), hero);
    QPushButton* ctaReports  = new QPushButton(QStringLiteral("View Reports"), hero);
    QPushButton* ctaInv      = new QPushButton(QStringLiteral("Inventory"), hero);
    QPushButton* ctaSettings = new QPushButton(QStringLiteral("Settings"), hero);
    ctaEntry->setObjectName(QStringLiteral("heroCta"));
    ctaReports->setObjectName(QStringLiteral("heroGhost"));
    ctaInv->setObjectName(QStringLiteral("heroGhost"));
    ctaSettings->setObjectName(QStringLiteral("heroGhost"));
    ctaLayout->addWidget(ctaEntry);
    ctaLayout->addWidget(ctaReports);
    ctaLayout->addWidget(ctaInv);
    ctaLayout->addWidget(ctaSettings);
    ctaLayout->addStretch(1);

    heroLayout->addWidget(kicker);
    heroLayout->addSpacing(12);
    heroLayout->addWidget(headline);
    heroLayout->addSpacing(6);
    heroLayout->addWidget(subline);
    heroLayout->addWidget(chipsRow);
    heroLayout->addWidget(ctaRow);
    layout->addWidget(hero);

    // ── Onboarding / quick-start steps ───────────────────────────────────────
    QFrame* onboard = createPanel(page);
    QVBoxLayout* onboardLayout = new QVBoxLayout(onboard);
    onboardLayout->setContentsMargins(24, 20, 24, 20);
    onboardLayout->setSpacing(0);

    QLabel* onboardTitle = new QLabel(QStringLiteral("Quick start"), onboard);
    onboardTitle->setObjectName(QStringLiteral("sectionTitle"));
    onboardLayout->addWidget(onboardTitle);
    onboardLayout->addSpacing(10);

    struct StepDef { QString num; QString title; QString desc; };
    const QList<StepDef> steps = {
        { QStringLiteral("1"), QStringLiteral("Connect to SQL Server"),
          QStringLiteral("Go to Settings → enter your server, database, username and password, then click Save.") },
        { QStringLiteral("2"), QStringLiteral("Add your parties"),
          QStringLiteral("Open Add Party to create customer and supplier names before entering transactions.") },
        { QStringLiteral("3"), QStringLiteral("Enter daily transactions"),
          QStringLiteral("Use Daily Entry for sales, purchases, receipts and expenses. Tab / Enter moves between fields.") },
        { QStringLiteral("4"), QStringLiteral("Review reports"),
          QStringLiteral("Party Ledger, Day Book, P&L and Trial Balance update instantly from your entries.") },
    };

    for (int i = 0; i < steps.size(); ++i) {
        const StepDef& step = steps[i];
        QFrame* stepRow = new QFrame(onboard);
        stepRow->setObjectName(QStringLiteral("onboardStep"));
        QHBoxLayout* stepLayout = new QHBoxLayout(stepRow);
        stepLayout->setContentsMargins(16, 14, 16, 14);
        stepLayout->setSpacing(16);

        QLabel* badge = new QLabel(step.num, stepRow);
        badge->setObjectName(QStringLiteral("stepBadge"));
        badge->setFixedSize(28, 28);
        badge->setAlignment(Qt::AlignCenter);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(3);
        QLabel* stepTitle = new QLabel(step.title, stepRow);
        QLabel* stepDesc  = new QLabel(step.desc, stepRow);
        stepTitle->setObjectName(QStringLiteral("stepTitle"));
        stepDesc->setObjectName(QStringLiteral("stepDesc"));
        stepDesc->setWordWrap(true);
        textLayout->addWidget(stepTitle);
        textLayout->addWidget(stepDesc);

        stepLayout->addWidget(badge);
        stepLayout->addLayout(textLayout, 1);
        onboardLayout->addWidget(stepRow);

        // Thin divider between steps (not after last)
        if (i < steps.size() - 1) {
            QFrame* div = new QFrame(onboard);
            div->setObjectName(QStringLiteral("divider"));
            div->setFrameShape(QFrame::HLine);
            div->setFixedHeight(1);
            onboardLayout->addWidget(div);
        }
    }
    layout->addWidget(onboard);

    // ── Tip label ─────────────────────────────────────────────────────────────
    QLabel* tipLabel = new QLabel(QStringLiteral("Press Ctrl+1..9 to jump between pages"), page);
    tipLabel->setObjectName(QStringLiteral("pageMeta"));
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);

    layout->addStretch(1);

    // ── Navigation wiring ─────────────────────────────────────────────────────
    auto goToPage = [&pages, &nav](int targetIndex) {
        for (int row = 0; row < nav.count(); ++row) {
            QListWidgetItem* item = nav.item(row);
            if (item && item->data(Qt::UserRole).toInt() == targetIndex) {
                nav.setCurrentRow(row);
                return;
            }
        }
        pages.setCurrentIndex(targetIndex);
    };
    connect(ctaEntry,    &QPushButton::clicked, this, [goToPage]() { goToPage(1); });
    connect(ctaReports,  &QPushButton::clicked, this, [goToPage]() { goToPage(3); });
    connect(ctaInv,      &QPushButton::clicked, this, [goToPage]() { goToPage(12); });
    connect(ctaSettings, &QPushButton::clicked, this, [goToPage, &pages]() { goToPage(pages.count() - 1); });

    // ── Staggered entrance animations ─────────────────────────────────────────
    animatePanel(*hero,     0);
    animatePanel(*onboard,  300);

    // Animate hero children individually for cascade effect
    animatePanel(*kicker, 100);
    animatePanel(*headline, 250);
    animatePanel(*subline, 350);
    animatePanel(*chipsRow, 450);
    animatePanel(*ctaRow, 550);

    // ── Variable font weight animation — headline starts thin, animates to bold ──
    QFont headlineFont = headline->font();
    headlineFont.setPointSize(28);
    headlineFont.setWeight(QFont::Thin);  // Start at weight 100
    headline->setFont(headlineFont);

    QVariantAnimation* weightAnim = new QVariantAnimation(headline);
    weightAnim->setStartValue(100);
    weightAnim->setEndValue(800);
    weightAnim->setDuration(800);
    weightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(weightAnim, &QVariantAnimation::valueChanged, headline, [headline](const QVariant& value) {
        QFont f = headline->font();
        f.setWeight(static_cast<QFont::Weight>(value.toInt()));
        headline->setFont(f);
    });
    QTimer::singleShot(200, weightAnim, [weightAnim]() { weightAnim->start(QAbstractAnimation::DeleteWhenStopped); });

    // Subtler weight animation on kicker text (300 → 600 over 600ms)
    QFont kickerFont = kicker->font();
    kickerFont.setWeight(QFont::Light);  // Start at weight 300
    kicker->setFont(kickerFont);

    QVariantAnimation* kickerWeightAnim = new QVariantAnimation(kicker);
    kickerWeightAnim->setStartValue(300);
    kickerWeightAnim->setEndValue(600);
    kickerWeightAnim->setDuration(600);
    kickerWeightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(kickerWeightAnim, &QVariantAnimation::valueChanged, kicker, [kicker](const QVariant& value) {
        QFont f = kicker->font();
        f.setWeight(static_cast<QFont::Weight>(value.toInt()));
        kicker->setFont(f);
    });
    QTimer::singleShot(100, kickerWeightAnim, [kickerWeightAnim]() { kickerWeightAnim->start(QAbstractAnimation::DeleteWhenStopped); });

    return page;
}

QWidget* DesktopApplication::buildDailyEntryPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    QWidget* header = new QWidget(page);
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(12);
    QWidget* titleBlock = new QWidget(header);
    QVBoxLayout* titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    QLabel* title = new QLabel(QStringLiteral("Daily Transactions"), titleBlock);
    QLabel* subtitle = new QLabel(QStringLiteral("%1 - %2").arg(currentCompanyText(context_), currentFinancialYearText()), titleBlock);
    title->setObjectName(QStringLiteral("pageTitle"));
    subtitle->setObjectName(QStringLiteral("pageMeta"));
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);
    headerLayout->addWidget(titleBlock, 1);
    const QStringList statusLabels = {
        QStringLiteral("Keyboard-Optimized"),
        QStringLiteral("SQL Server Connected"),
        QStringLiteral("Native Runtime Active"),
        QStringLiteral("Live Reports")
    };
    for (const QString& statusLabel : statusLabels) {
        QLabel* label = new QLabel(statusLabel, header);
        label->setObjectName(QStringLiteral("contextChip"));
        headerLayout->addWidget(label);
    }
    layout->addWidget(header);

    QFrame* entryPanel = new QFrame(page);
    entryPanel->setObjectName(QStringLiteral("formPanel"));
    entryPanel->setFrameShape(QFrame::NoFrame);
    QGridLayout* form = new QGridLayout(entryPanel);
    form->setContentsMargins(16, 16, 16, 16);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    QDateEdit* date = new QDateEdit(QDate::currentDate(), entryPanel);
    date->setCalendarPopup(true);
    date->setDisplayFormat(QStringLiteral("dd-MM-yyyy"));
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
    deleteButton->hide();
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
    form->addWidget(clear, 4, 4);
    form->setColumnStretch(0, 1);
    form->setColumnStretch(1, 2);
    form->setColumnStretch(2, 2);
    form->setColumnStretch(3, 1);
    form->setColumnStretch(4, 1);
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
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->setSpacing(0);
    QHBoxLayout* tableHeader = new QHBoxLayout();
    tableHeader->setContentsMargins(14, 10, 12, 6);
    tableHeader->addWidget(createSectionTitle(QStringLiteral("Recent Entries"), tablePanel));
    tableHeader->addStretch(1);
    QLineEdit* transactionSearch = new QLineEdit(tablePanel);
    transactionSearch->setPlaceholderText(QStringLiteral("Search"));
    transactionSearch->setFixedWidth(200);
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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);

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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);
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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);

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
    connect(search, &QLineEdit::textChanged, this, [this, table, title, search, start, end](const QString& query) {
        if (title == QStringLiteral("Party Ledger")) {
            const domain::ReportRange range{start->date(), end->date(), rangeDays(start->date(), end->date())};
            loadReportTable(*table, title, query.trimmed(), range);
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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);
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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);
    layout->addWidget(createPageHeader(QStringLiteral("Settings"), QStringLiteral("Database configuration, backup paths, and native runtime controls."), page));
    layout->addWidget(createContextBar(context_, page));

    // Theme toggle
    QFrame* themePanel = createPanel(page);
    QHBoxLayout* themeLayout = new QHBoxLayout(themePanel);
    themeLayout->setContentsMargins(14, 12, 14, 12);
    themeLayout->setSpacing(10);
    QLabel* themeLabel = new QLabel(QStringLiteral("Appearance"), themePanel);
    themeLabel->setObjectName(QStringLiteral("sectionTitle"));
    QComboBox* themeCombo = new QComboBox(themePanel);
    themeCombo->addItems({QStringLiteral("Light"), QStringLiteral("Dark")});
    themeCombo->setCurrentIndex(0);
    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(themeCombo);
    themeLayout->addStretch(1);
    connect(themeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool isDark = (index == 1);
        applyTheme(isDark);
    });
    layout->addWidget(themePanel);

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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);
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
    layout->setContentsMargins(20, 14, 20, 22);
    layout->setSpacing(6);
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
    const QStringList headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")};
    ensureTableHeaders(table, headers);
    try {
        const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 100, 365);
        QJsonArray data;
        for (const domain::TransactionRow& row : rows) {
            data.append(transactionToJson(row));
        }
        setTableRows(table, headers, data);
        table.setColumnWidth(0, 120);
        table.setColumnWidth(1, 150);
        table.setColumnWidth(2, 260);
        table.setColumnWidth(3, 120);
        table.setColumnWidth(4, 120);
        table.setColumnWidth(5, 120);
    } catch (const std::exception& err) {
        showError(QStringLiteral("Transactions"), err);
    }
}

void DesktopApplication::loadParties(QTableWidget& table) {
    const QStringList headers = {QStringLiteral("name"), QStringLiteral("type")};
    ensureTableHeaders(table, headers);
    try {
        setTableRows(table, headers, context_.services().parties->listParties());
    } catch (const std::exception& err) {
        showError(QStringLiteral("Parties"), err);
    }
}

void DesktopApplication::loadInventorySnapshot(QTableWidget& table, const QString& financialYear, int month) {
    // Step 1 — Build 66-element header list
    const int visibleDays = daysInInventoryMonth(month);
    QStringList headers;
    headers.reserve(66);
    headers << QStringLiteral("row_id") << QStringLiteral("Product") << QStringLiteral("Cost") << QStringLiteral("Min Stock");
    for (int day = 1; day <= 31; ++day) {
        headers.append(QString::asprintf("%02d Qty", day));
    }
    for (int day = 1; day <= 31; ++day) {
        headers.append(QString::asprintf("%02d In", day));
    }

    // Step 2 — Reset table state BEFORE setting headers
    table.clearSpans();
    table.clear();
    table.setRowCount(0);
    table.setColumnCount(66);

    // Step 3 — Set headers FIRST (before any hide/width calls)
    table.setHorizontalHeaderLabels(headers);

    // Step 4 — Configure column visibility and widths
    table.setColumnHidden(0, true);   // hide row_id
    table.setColumnWidth(1, 230);     // Product
    table.setColumnWidth(2, 95);      // Cost
    table.setColumnWidth(3, 100);     // Min Stock
    for (int day = 1; day <= 31; ++day) {
        const bool visible = (day <= visibleDays);
        table.setColumnHidden(3 + day, !visible);
        table.setColumnHidden(34 + day, !visible);
        if (visible) {
            table.setColumnWidth(3 + day, 78);
            table.setColumnWidth(34 + day, 72);
        }
    }

    // Step 5 — Load data from service (wrapped in try/catch)
    QJsonArray rows;
    try {
        rows = context_.services().inventory->loadSnapshot(financialYear, month);
    } catch (const std::exception& e) {
        showError(tr("Failed to load inventory: %1").arg(e.what()), e);
        table.horizontalHeader()->resizeSections(QHeaderView::Fixed);
        return;
    }

    // Step 6 — Handle empty result
    if (rows.isEmpty()) {
        table.setRowCount(1);
        auto* placeholder = new QTableWidgetItem(tr("No inventory rows yet..."));
        placeholder->setFlags(Qt::ItemIsEnabled);
        placeholder->setForeground(QColor(QStringLiteral("#58677a")));
        table.setItem(0, 1, placeholder);
        table.setSpan(0, 1, 1, 64);
        table.horizontalHeader()->resizeSections(QHeaderView::Fixed);
        return;
    }

    // Step 7 — Populate rows
    table.setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const QJsonObject row = rows[r].toObject();
        // col 0: row_id (read-only, hidden)
        auto* idItem = new QTableWidgetItem(row[QStringLiteral("row_id")].toVariant().toString());
        idItem->setFlags(Qt::ItemIsEnabled);
        table.setItem(r, 0, idItem);
        // col 1: Product name (editable)
        auto* nameItem = new QTableWidgetItem(row[QStringLiteral("name")].toString());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        table.setItem(r, 1, nameItem);
        // col 2: Cost (editable, right-aligned)
        auto* costItem = new QTableWidgetItem(row[QStringLiteral("cost")].toVariant().toString());
        costItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table.setItem(r, 2, costItem);
        // col 3: Min Stock (editable, right-aligned)
        auto* minItem = new QTableWidgetItem(row[QStringLiteral("min_stock")].toVariant().toString());
        minItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        minItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table.setItem(r, 3, minItem);
        // cols 4..34: Qty per day
        for (int day = 1; day <= 31; ++day) {
            const QString key = QString::asprintf("qty_%02d", day);
            const double qty = row[key].toDouble();
            const QString text = (qty == 0.0) ? QString{} : QString::number(qty, 'f', 2);
            auto* item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table.setItem(r, 3 + day, item);
        }
        // cols 35..65: Purchase/In per day
        for (int day = 1; day <= 31; ++day) {
            const QString key = QString::asprintf("purchase_%02d", day);
            const double purchase = row[key].toDouble();
            const QString text = (purchase == 0.0) ? QString{} : QString::number(purchase, 'f', 2);
            auto* item = new QTableWidgetItem(text);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table.setItem(r, 34 + day, item);
        }
    }

    // Step 8 — Force header repaint AFTER all rows populated
    table.horizontalHeader()->resizeSections(QHeaderView::Fixed);
}

void DesktopApplication::loadInventoryValue(QTableWidget& table, const QString& financialYear, int month) {
    const QStringList headers = {QStringLiteral("day"), QStringLiteral("quantity"), QStringLiteral("stock_value")};
    ensureTableHeaders(table, headers);
    try {
        setTableRows(table, headers, context_.services().inventory->stockValue(financialYear, month));
    } catch (const std::exception& err) {
        showError(QStringLiteral("Stock Value"), err);
    }
}

void DesktopApplication::loadAuditLogs(QTableWidget& table) {
    const QStringList headers = {QStringLiteral("timestamp"), QStringLiteral("username"), QStringLiteral("action"), QStringLiteral("details")};
    ensureTableHeaders(table, headers);
    try {
        setTableRows(table, headers, context_.services().audit->listAuditLogs(QStringLiteral("native")));
    } catch (const std::exception& err) {
        showError(QStringLiteral("Audit Logs"), err);
    }
}

void DesktopApplication::loadReportTable(QTableWidget& table, const QString& reportName, const QString& partyName, const domain::ReportRange& range) {
    QStringList headers;
    if (reportName == QStringLiteral("Party Ledger")) {
        headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount"), QStringLiteral("balance")};
    } else if (reportName == QStringLiteral("Day Book")) {
        headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Purchase Report") || reportName == QStringLiteral("Expenses")) {
        headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Outstanding")) {
        headers = {QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("balance")};
    } else if (reportName == QStringLiteral("Trial Balance")) {
        headers = {QStringLiteral("account"), QStringLiteral("debit"), QStringLiteral("credit"), QStringLiteral("balance")};
    } else if (reportName == QStringLiteral("Profit & Loss")) {
        headers = {QStringLiteral("metric"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Daily Summary")) {
        headers = {QStringLiteral("date"), QStringLiteral("sales"), QStringLiteral("sale_returns"), QStringLiteral("receipts"), QStringLiteral("expenses"), QStringLiteral("net_sales")};
    } else if (reportName == QStringLiteral("Short / Excess")) {
        headers = {QStringLiteral("date"), QStringLiteral("cash_in_hand")};
    }
    if (!headers.isEmpty()) {
        ensureTableHeaders(table, headers);
    }
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
                setTableRows(table, headers, rows);
                return;
            }
            const QJsonObject report = context_.services().reports->ledger(partyName, range);
            setTableRows(table, headers, report.value(QStringLiteral("data")).toArray());
            return;
        }
        if (reportName == QStringLiteral("Day Book")) {
            setTableRows(table, headers, context_.services().reports->dayBook(range.end));
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
            setTableRows(table, headers, data);
            return;
        }
        if (reportName == QStringLiteral("Outstanding")) {
            const QJsonObject report = context_.services().reports->outstanding();
            setTableRows(table, headers, report.value(QStringLiteral("data")).toArray());
            return;
        }
        if (reportName == QStringLiteral("Trial Balance")) {
            setTableRows(table, headers, context_.services().reports->trialBalance());
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
            setTableRows(table, headers, rows);
            return;
        }

        if (reportName == QStringLiteral("Daily Summary")) {
            setTableRows(table, headers, context_.services().reports->dailySummary(range));
            return;
        }
        if (reportName == QStringLiteral("Short / Excess")) {
            setTableRows(table, headers, context_.services().reports->shortExcess(range));
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
    const int previousColumns = table.property("columnCount").toInt();
    const bool shouldAutoSize = previousColumns != headers.size() || !table.property("columnsSized").toBool();
    table.clear();
    table.setColumnCount(headers.size());
    table.setHorizontalHeaderLabels(humanizeHeaderKeys(headers));
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
    if (shouldAutoSize) {
        table.resizeColumnsToContents();
        table.horizontalHeader()->setStretchLastSection(true);
        table.setProperty("columnsSized", true);
    }
    table.setProperty("columnCount", headers.size());
}

void DesktopApplication::showError(const QString& title, const std::exception& err) {
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, QString::fromUtf8(err.what())), 8000);
}

void DesktopApplication::wireActions() {
    QToolBar* toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    QLabel* title = new QLabel(QStringLiteral("M-Finlogs Native"), toolbar);
    title->setObjectName(QStringLiteral("toolbarTitle"));
    QWidget* spacer = new QWidget(toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(title);
    toolbar->addWidget(spacer);
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

    // Show the modern frameless splash first. The main window (and its auth
    // dialog) is only constructed once the splash animation completes, so the
    // splash never covers the modal sign-in dialog.
    auto splash = std::make_unique<SplashScreen>();
    std::unique_ptr<AppContext> context;
    std::unique_ptr<DesktopApplication> window;

    splash->show();
    splash->setProgress(20, QStringLiteral("Loading services..."));

    QObject::connect(splash.get(), &SplashScreen::finished, &qtApp, [&]() {
        context = std::make_unique<AppContext>(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
        window = std::make_unique<DesktopApplication>(*context);
        splash->close();
        window->show();
        window->raise();
        window->activateWindow();
    });
    splash->runIndeterminate(1400);

    return qtApp.exec();
}

} // namespace mfinlogs::app
