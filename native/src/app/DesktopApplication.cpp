#include "app/DesktopApplication.h"
#include "app/XlsxReader.h"

#include "app/SplashScreen.h"
#include "domain/Types.h"

#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QColor>
#include <QFormLayout>
#include <QFont>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QProcess>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardPaths>
#include <QStringList>
#include <QStringConverter>
#include <QStyle>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTableWidget>
#include <QTextStream>
#include <QToolBar>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>

#include <exception>
#include <algorithm>
#include <memory>
#include <optional>
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

struct InventoryMailProfile final {
    QString toEmail;
    QString ccEmail;
    QString senderEmail;
    QString smtpHost;
    QString smtpPort;
    QString subject;
    QString averageMode;
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
    try {
        const mfinlogs::domain::DatabaseConfig config = context.services().config->readDatabaseConfig();
        return QStringLiteral("Database: %1").arg(config.database.trimmed().isEmpty() ? QStringLiteral("Finlogs") : config.database);
    } catch (const std::exception&) {
        return QStringLiteral("Database: Not configured");
    }
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
    table->horizontalHeader()->setMinimumSectionSize(50);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->installEventFilter(new InventoryTableKeyFilter(*table));
    return table;
}

// Number of frozen columns in the inventory table (row_id hidden + Product + Cost + Min Stock)
constexpr int kInventoryFrozenCols = 4;

// Creates a frozen-pane overlay for the inventory table. The frozen table shows
// columns 0..kInventoryFrozenCols-1 and stays fixed while the main table scrolls.
QTableWidget* createFrozenOverlay(QTableWidget* mainTable) {
    QTableWidget* frozen = new QTableWidget(mainTable->parentWidget());
    frozen->setObjectName(QStringLiteral("inventoryTable"));
    frozen->setAlternatingRowColors(true);
    frozen->setSelectionBehavior(QAbstractItemView::SelectItems);
    frozen->setSelectionMode(QAbstractItemView::SingleSelection);
    frozen->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    frozen->horizontalHeader()->setStretchLastSection(false);
    frozen->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    frozen->horizontalHeader()->setMinimumSectionSize(50);
    frozen->verticalHeader()->setVisible(false);
    frozen->verticalHeader()->setDefaultSectionSize(28);
    frozen->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozen->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozen->setFrameShape(QFrame::NoFrame);
    frozen->setStyleSheet(QStringLiteral(
        "QTableWidget { background: #ffffff; border-right: 2px solid #6366f1; border-radius: 0; }"));
    return frozen;
}

// Synchronize the frozen overlay geometry and vertical scroll with the main table.
void setupFrozenPane(QTableWidget* mainTable, QTableWidget* frozenTable) {
    auto updateFrozenGeometry = [mainTable, frozenTable]() {
        int frozenWidth = 0;
        for (int col = 0; col < kInventoryFrozenCols; ++col) {
            if (!mainTable->isColumnHidden(col)) {
                frozenWidth += mainTable->columnWidth(col);
            }
        }
        const int headerH = mainTable->horizontalHeader()->height();
        frozenTable->setGeometry(
            mainTable->x(), mainTable->y(),
            frozenWidth + mainTable->verticalHeader()->width(),
            mainTable->height());
    };

    // Sync vertical scroll
    QObject::connect(mainTable->verticalScrollBar(), &QScrollBar::valueChanged,
        frozenTable->verticalScrollBar(), &QScrollBar::setValue);
    QObject::connect(frozenTable->verticalScrollBar(), &QScrollBar::valueChanged,
        mainTable->verticalScrollBar(), &QScrollBar::setValue);

    // Update geometry when main table resizes
    QObject::connect(mainTable->horizontalHeader(), &QHeaderView::sectionResized,
        mainTable, [updateFrozenGeometry](int, int, int) { updateFrozenGeometry(); });

    // Initial geometry
    QTimer::singleShot(0, mainTable, updateFrozenGeometry);
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
    item->setData(Qt::UserRole + 1, label);
    item->setToolTip(label);
    return item;
}

QIcon appIcon(const QString& name) {
    // Render SVG directly via QSvgRenderer (Qt6::Svg) to avoid depending on the
    // qsvg image plugin being deployed.
    const QStringList candidates = {
        QStringLiteral(":/icons/%1.svg").arg(name),
        QStringLiteral(":/icons/icons/%1.svg").arg(name),
    };
    for (const QString& path : candidates) {
        if (!QFile::exists(path)) {
            continue;
        }
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) {
            continue;
        }
        QPixmap pixmap(40, 40);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter);
        painter.end();
        return QIcon(pixmap);
    }
    return QIcon();
}

// Render an SVG icon tinted to a single color, for display on the dark sidebar.
QIcon appIconTinted(const QString& name, const QColor& color) {
    const QStringList candidates = {
        QStringLiteral(":/icons/%1.svg").arg(name),
        QStringLiteral(":/icons/icons/%1.svg").arg(name),
    };
    for (const QString& path : candidates) {
        if (!QFile::exists(path)) {
            continue;
        }
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) {
            continue;
        }
        QPixmap pixmap(40, 40);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter);
        // Tint: paint color over the icon shape using SourceIn composition
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }
    return QIcon();
}

QListWidgetItem* addNavItem(QListWidget& nav, const QString& label, int pageIndex, const QString& iconName) {
    QListWidgetItem* item = addNavItem(nav, label, pageIndex);
    // Sidebar is dark, so tint icons light for visibility
    item->setIcon(appIconTinted(iconName, QColor(QStringLiteral("#aeb8d4"))));
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

QStringList parseCsvLine(const QString& line) {
    QStringList values;
    QString current;
    bool quoted = false;
    for (int index = 0; index < line.size(); index += 1) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('"')) {
            if (quoted && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                current.append(QLatin1Char('"'));
                index += 1;
            } else {
                quoted = !quoted;
            }
        } else if (character == QLatin1Char(',') && !quoted) {
            values.append(current.trimmed());
            current.clear();
        } else {
            current.append(character);
        }
    }
    values.append(current.trimmed());
    return values;
}

int daysInInventoryMonth(int month) {
    // Use a fixed non-leap year (2023) for consistent day counts across all financial years.
    // April-December map to 2023; January-March map to 2024 (next calendar year in FY).
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
    return (QString(QChar(0x20B9)) + QStringLiteral("%1")).arg(value, 0, 'f', 2);
}

bool isMoneyColumn(const QString& key) {
    const QString normalized = key.trimmed().toLower();
    return normalized == QStringLiteral("amount")
        || normalized == QStringLiteral("balance")
        || normalized == QStringLiteral("debit")
        || normalized == QStringLiteral("credit")
        || normalized == QStringLiteral("sales")
        || normalized == QStringLiteral("sale_returns")
        || normalized == QStringLiteral("receipts")
        || normalized == QStringLiteral("expenses")
        || normalized == QStringLiteral("net_sales")
        || normalized == QStringLiteral("net_profit")
        || normalized == QStringLiteral("opening_cash")
        || normalized == QStringLiteral("cash_in")
        || normalized == QStringLiteral("cash_expense")
        || normalized == QStringLiteral("cash_needed")
        || normalized == QStringLiteral("cash_in_hand")
        || normalized == QStringLiteral("cash_short_excess")
        || normalized == QStringLiteral("bank")
        || normalized == QStringLiteral("credit_sale")
        || normalized == QStringLiteral("total_sales")
        || normalized == QStringLiteral("total_purchase")
        || normalized == QStringLiteral("stock_value");
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

// Premium "Aurora Glass" theme: frosted translucent surfaces, layered gradient
// backdrops, indigo/blue accent system, and refined typography. Light and dark
// variants are emitted as complete stylesheets to keep the generator robust.
static QString buildModernQss(bool darkMode = false) {
    static QString cachedDark;
    static QString cachedLight;
    if (darkMode) {
        if (!cachedDark.isEmpty()) return cachedDark;
        cachedDark = QStringLiteral(
            // Base + typography
            "* { font-family: 'Inter Tight','Segoe UI','-apple-system','Helvetica Neue',sans-serif; font-weight: 400; outline: none; }"
            "QMainWindow { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #090e1c, stop:0.5 #0b1326, stop:1 #0a1322); }"
            "QWidget#workspace { background: transparent; color: #e8edf9; font-size: 13px; }"
            "QLabel { color: #e8edf9; background: transparent; }"
            "QToolTip { background: #11192e; color: #e8edf9; border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; padding: 6px 9px; font-size: 12px; }"
            // Sidebar
            "QWidget#sidebarWrap, QWidget#sidebarWrapCollapsed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #0d1426, stop:0.5 #0a1120, stop:1 #070c18); border: none; border-right: 1px solid rgba(255,255,255,0.05); }"
            "QLabel#brandLabel { color: #f2f5fc; font-size: 16px; font-weight: 800; letter-spacing: 0.3px; padding: 12px 16px 10px 16px; }"
            "QLabel#brandSub { color: #6b7894; font-size: 11px; padding: 0 16px 10px 16px; }"
            "QPushButton#sidebarToggle { background: rgba(255,255,255,0.07); color: #aeb8d4; border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; min-height: 28px; padding: 0 8px; font-weight: 800; }"
            "QPushButton#sidebarToggle:hover { background: rgba(255,255,255,0.16); }"
            "QPushButton#sidebarToggleCollapsed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border: none; border-radius: 8px; min-height: 28px; padding: 0 8px; font-weight: 900; }"
            "QPushButton#sidebarToggleCollapsed:hover { background: #6366f1; }"
            "QListWidget#sidebar { background: transparent; border: none; color: #aeb8d4; font-size: 13px; font-weight: 500; outline: none; padding: 6px 0; }"
            "QListWidget#sidebar::item { padding: 9px 12px; border-radius: 10px; min-height: 22px; margin: 2px 10px; color: #aeb8d4; }"
            "QListWidget#sidebar::item:selected { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; font-weight: 700; }"
            "QListWidget#sidebar::item:hover:!selected { background: rgba(255,255,255,0.07); color: #e8edf9; }"
            // Typography roles
            "QLabel#pageTitle { color: #f4f7ff; font-size: 22px; font-weight: 800; letter-spacing: -0.5px; }"
            "QLabel#pageMeta { color: #9aa6c0; font-size: 12px; }"
            "QLabel#sectionTitle { color: #eef2fb; font-size: 15px; font-weight: 800; }"
            "QLabel#sectionDescription { color: #9aa6c0; font-size: 12px; }"
            "QLabel#metricLabel { color: #9aa6c0; font-size: 11px; font-weight: 600; letter-spacing: 0.4px; }"
            "QLabel#metricValue { color: #f4f7ff; font-size: 22px; font-weight: 800; letter-spacing: -0.5px; }"
            "QLabel#contextChip { color: #c5cee4; background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.1); border-radius: 11px; padding: 3px 11px; font-size: 12px; font-weight: 600; }"
            // Frosted panels
            "QFrame#panel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(34,45,74,0.78), stop:1 rgba(20,28,50,0.6)); border: 1px solid rgba(255,255,255,0.08); border-radius: 16px; }"
            "QFrame#formPanel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(34,45,74,0.85), stop:1 rgba(20,28,50,0.7)); border: 1px solid rgba(255,255,255,0.09); border-radius: 16px; }"
            "QFrame#accentPanel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 rgba(99,102,241,0.22), stop:1 rgba(91,140,250,0.14)); border: 1px solid rgba(99,102,241,0.3); border-radius: 12px; }"
            "QFrame#contextBar { background: transparent; border: none; padding: 0; }"
            "QFrame#divider { background: rgba(255,255,255,0.08); border: none; max-height: 1px; }"
            // Tables
            "QTableWidget#dataTable, QTableWidget#inventoryTable { background: rgba(17,24,43,0.92); alternate-background-color: rgba(28,37,62,0.55); border: none; border-radius: 0px; selection-background-color: rgba(99,102,241,0.4); selection-color: #ffffff; gridline-color: rgba(255,255,255,0.06); color: #dbe3f4; font-size: 13px; font-weight: 500; }"
            "QTableWidget#dataTable::item, QTableWidget#inventoryTable::item { padding: 7px 10px; border-bottom: 1px solid rgba(255,255,255,0.05); color: #dbe3f4; }"
            "QTableWidget#inventoryTable::item { padding: 4px 6px; border-right: 1px solid rgba(255,255,255,0.05); }"
            "QTableWidget::item:selected { background: rgba(99,102,241,0.4); color: #ffffff; }"
            "QHeaderView { background-color: transparent; }"
            "QHeaderView::section { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1c2540, stop:1 #141b30); color: #cdd6ee; border: none; border-right: 1px solid rgba(255,255,255,0.05); border-bottom: 2px solid #6366f1; padding: 9px 12px; font-weight: 700; font-size: 12px; letter-spacing: 0.3px; }"
            "QTableCornerButton::section { background: #141b30; border: none; }"
            // Buttons
            "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border: none; border-radius: 11px; min-height: 34px; padding: 2px 20px; font-weight: 700; font-size: 13px; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #6f9bff, stop:1 #7174f4); }"
            "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4571e6, stop:1 #4a4ddb); }"
            "QPushButton:disabled { background: rgba(255,255,255,0.06); color: #5b6b8a; }"
            "QPushButton#secondaryButton { background: rgba(255,255,255,0.06); color: #aab6ff; border: 1px solid rgba(99,102,241,0.4); }"
            "QPushButton#secondaryButton:hover { background: rgba(99,102,241,0.18); border-color: #6366f1; }"
            "QPushButton#dangerButton { background: rgba(180,35,24,0.18); color: #ff9b91; border: 1px solid rgba(255,107,93,0.4); }"
            "QPushButton#dangerButton:hover { background: rgba(180,35,24,0.3); }"
            // Inputs
            "QLineEdit, QDateEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.12); border-radius: 11px; min-height: 34px; padding: 0 12px; color: #e8edf9; font-size: 13px; selection-background-color: rgba(99,102,241,0.45); selection-color: #ffffff; }"
            "QLineEdit:hover, QDateEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover, QComboBox:hover { border-color: rgba(255,255,255,0.22); }"
            "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus { border-color: #6366f1; background: rgba(99,102,241,0.1); }"
            "QLineEdit:disabled, QComboBox:disabled { background: rgba(255,255,255,0.03); color: #5b6b8a; }"
            "QComboBox::drop-down { border: none; width: 22px; }"
            "QComboBox QAbstractItemView { background: #131a2e; border: 1px solid rgba(255,255,255,0.12); border-radius: 10px; selection-background-color: rgba(99,102,241,0.45); selection-color: #ffffff; outline: none; padding: 4px; color: #e8edf9; }"
            "QDateEdit::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button, QSpinBox::up-button, QSpinBox::down-button { border: none; width: 18px; background: transparent; }"
            // Checkbox
            "QCheckBox { color: #e8edf9; spacing: 8px; font-size: 13px; }"
            "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 6px; border: 1px solid rgba(255,255,255,0.22); background: rgba(255,255,255,0.05); }"
            "QCheckBox::indicator:hover { border-color: #6366f1; }"
            "QCheckBox::indicator:checked { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); border: none; }"
            // Scrollbars
            "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
            "QScrollBar::handle:vertical { background: rgba(99,102,241,0.4); border-radius: 5px; min-height: 28px; }"
            "QScrollBar::handle:vertical:hover { background: rgba(99,102,241,0.65); }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
            "QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }"
            "QScrollBar::handle:horizontal { background: rgba(99,102,241,0.4); border-radius: 5px; min-width: 28px; }"
            "QScrollBar::handle:horizontal:hover { background: rgba(99,102,241,0.65); }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
            // Chrome
            "QStatusBar { background: rgba(255,255,255,0.03); color: #9aa6c0; border-top: 1px solid rgba(255,255,255,0.07); font-size: 12px; }"
            "QStatusBar::item { border: none; }"
            "QDialog, QMessageBox { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #0b1326, stop:1 #0d1224); }"
            "QToolBar { background: rgba(255,255,255,0.03); border: none; border-bottom: 1px solid rgba(255,255,255,0.07); spacing: 10px; padding: 0 14px; min-height: 46px; }"
            "QToolBar QLabel { color: #e8edf9; font-size: 13px; }"
            "QLabel#toolbarTitle { color: #f4f7ff; font-size: 14px; font-weight: 800; }"
            "QToolButton { background: transparent; color: #e8edf9; border: none; padding: 6px 8px; border-radius: 8px; font-size: 13px; }"
            "QToolButton:hover { background: rgba(99,102,241,0.18); }"
            "QMenu { background: #131a2e; border: 1px solid rgba(255,255,255,0.12); border-radius: 10px; padding: 6px; color: #e8edf9; }"
            "QMenu::item { padding: 7px 18px; border-radius: 7px; color: #e8edf9; }"
            "QMenu::item:selected { background: rgba(99,102,241,0.4); }"
            "QSplitter::handle { background: rgba(99,102,241,0.25); }"
            // Welcome hero
            "QFrame#welcomeHero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #312e81, stop:0.45 #3730a3, stop:1 #2563eb); border-radius: 20px; border: 1px solid rgba(255,255,255,0.14); }"
            "QLabel#welcomeKicker { color: rgba(255,255,255,0.72); font-size: 11px; font-weight: 800; letter-spacing: 1.5px; background: transparent; }"
            "QLabel#welcomeTitle { color: #ffffff; font-size: 34px; font-weight: 800; letter-spacing: -0.5px; background: transparent; }"
            "QLabel#welcomeSubtitle { color: rgba(255,255,255,0.82); font-size: 13px; background: transparent; }"
            "QLabel#welcomeStat { color: #ffffff; background: rgba(255,255,255,0.14); border: 1px solid rgba(255,255,255,0.24); border-radius: 13px; padding: 6px 13px; font-size: 12px; font-weight: 700; }"
            "QPushButton#heroCta { background: #ffffff; color: #4f46e5; border: none; border-radius: 11px; min-height: 36px; padding: 4px 22px; font-weight: 800; font-size: 13px; }"
            "QPushButton#heroCta:hover { background: #eef0ff; }"
            "QPushButton#heroCta:pressed { background: #e2e5ff; }"
            "QPushButton#heroGhost { background: rgba(255,255,255,0.1); color: #ffffff; border: 1px solid rgba(255,255,255,0.32); border-radius: 11px; min-height: 36px; padding: 4px 22px; font-weight: 700; font-size: 13px; }"
            "QPushButton#heroGhost:hover { background: rgba(255,255,255,0.2); }"
            "QPushButton#heroGhost:pressed { background: rgba(255,255,255,0.28); }"
            // Feature cards & onboarding
            "QFrame#featureCard { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.08); border-radius: 14px; }"
            "QFrame#featureCard:hover { background: rgba(99,102,241,0.14); border-color: rgba(99,102,241,0.4); }"
            "QLabel#featureIcon { color: #8ea2ff; font-size: 22px; }"
            "QLabel#featureTitle { color: #eef2fb; font-size: 13px; font-weight: 700; }"
            "QLabel#featureDesc { color: #9aa6c0; font-size: 12px; }"
            "QFrame#onboardStep { background: transparent; border: none; border-radius: 12px; }"
            "QFrame#onboardStep:hover { background: rgba(99,102,241,0.1); }"
            "QLabel#stepBadge { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border-radius: 14px; font-size: 12px; font-weight: 800; min-width: 28px; max-width: 28px; min-height: 28px; max-height: 28px; qproperty-alignment: AlignCenter; }"
            "QLabel#stepTitle { color: #eef2fb; font-size: 14px; font-weight: 800; }"
            "QLabel#stepDesc { color: #9aa6c0; font-size: 12px; }"
            "QLabel#formStatus { color: #9aa6c0; font-size: 12px; font-weight: 700; }"
        );
    }

    if (!cachedLight.isEmpty()) return cachedLight;
    cachedLight = QStringLiteral(
        // Base + typography
        "* { font-family: 'Inter Tight','Segoe UI','-apple-system','Helvetica Neue',sans-serif; font-weight: 400; outline: none; }"
        "QMainWindow { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #e7ecfb, stop:0.5 #eae8fa, stop:1 #e8f0fb); }"
        "QWidget#workspace { background: transparent; color: #1e2435; font-size: 13px; }"
        "QLabel { color: #1e2435; background: transparent; }"
        "QToolTip { background: #1b2440; color: #eef2fb; border: 1px solid rgba(255,255,255,0.12); border-radius: 8px; padding: 6px 9px; font-size: 12px; }"
        // Sidebar (deep indigo glass)
        "QWidget#sidebarWrap, QWidget#sidebarWrapCollapsed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #1b2440, stop:0.5 #161e38, stop:1 #0f1730); border: none; border-right: 1px solid rgba(255,255,255,0.06); }"
        "QLabel#brandLabel { color: #f2f5fc; font-size: 16px; font-weight: 800; letter-spacing: 0.3px; padding: 12px 16px 10px 16px; }"
        "QLabel#brandSub { color: #8591ad; font-size: 11px; padding: 0 16px 10px 16px; }"
        "QPushButton#sidebarToggle { background: rgba(255,255,255,0.08); color: #c7d0e4; border: 1px solid rgba(255,255,255,0.14); border-radius: 8px; min-height: 28px; padding: 0 8px; font-weight: 800; }"
        "QPushButton#sidebarToggle:hover { background: rgba(255,255,255,0.16); }"
        "QPushButton#sidebarToggleCollapsed { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border: none; border-radius: 8px; min-height: 28px; padding: 0 8px; font-weight: 900; }"
        "QPushButton#sidebarToggleCollapsed:hover { background: #6366f1; }"
        "QListWidget#sidebar { background: transparent; border: none; color: #c7d0e4; font-size: 13px; font-weight: 500; outline: none; padding: 6px 0; }"
        "QListWidget#sidebar::item { padding: 9px 12px; border-radius: 10px; min-height: 22px; margin: 2px 10px; color: #c2cbe0; }"
        "QListWidget#sidebar::item:selected { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; font-weight: 700; }"
        "QListWidget#sidebar::item:hover:!selected { background: rgba(255,255,255,0.08); color: #eef2fb; }"
        // Typography roles
        "QLabel#pageTitle { color: #161c2d; font-size: 22px; font-weight: 800; letter-spacing: -0.5px; }"
        "QLabel#pageMeta { color: #64748b; font-size: 12px; }"
        "QLabel#sectionTitle { color: #1e2435; font-size: 15px; font-weight: 800; }"
        "QLabel#sectionDescription { color: #64748b; font-size: 12px; }"
        "QLabel#metricLabel { color: #64748b; font-size: 11px; font-weight: 600; letter-spacing: 0.4px; }"
        "QLabel#metricValue { color: #161c2d; font-size: 22px; font-weight: 800; letter-spacing: -0.5px; }"
        "QLabel#contextChip { color: #475569; background: rgba(255,255,255,0.6); border: 1px solid rgba(148,163,184,0.3); border-radius: 11px; padding: 3px 11px; font-size: 12px; font-weight: 600; }"
        // Frosted panels
        "QFrame#panel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(255,255,255,0.82), stop:1 rgba(255,255,255,0.62)); border: 1px solid rgba(255,255,255,0.7); border-radius: 16px; }"
        "QFrame#formPanel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(255,255,255,0.95), stop:1 rgba(255,255,255,0.82)); border: 1px solid rgba(148,163,184,0.28); border-radius: 16px; }"
        "QFrame#accentPanel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 rgba(99,102,241,0.1), stop:1 rgba(91,140,250,0.08)); border: 1px solid rgba(99,102,241,0.18); border-radius: 12px; }"
        "QFrame#contextBar { background: transparent; border: none; padding: 0; }"
        "QFrame#divider { background: rgba(148,163,184,0.28); border: none; max-height: 1px; }"
        // Tables
        "QTableWidget#dataTable, QTableWidget#inventoryTable { background: #ffffff; alternate-background-color: #f5f7fd; border: none; border-radius: 0px; selection-background-color: #e0e7ff; selection-color: #1e2435; gridline-color: #eaeef7; color: #1e2435; font-size: 13px; font-weight: 500; }"
        "QTableWidget#dataTable::item, QTableWidget#inventoryTable::item { padding: 7px 10px; border-bottom: 1px solid #eef1f8; color: #1e2435; }"
        "QTableWidget#inventoryTable::item { padding: 4px 6px; border-right: 1px solid #eef1f8; }"
        "QTableWidget::item:selected { background: #e0e7ff; color: #1e2435; }"
        "QHeaderView { background-color: transparent; }"
        "QHeaderView::section { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #232c4a, stop:1 #1a2238); color: #eef2fb; border: none; border-right: 1px solid rgba(255,255,255,0.06); border-bottom: 2px solid #6366f1; padding: 9px 12px; font-weight: 700; font-size: 12px; letter-spacing: 0.3px; }"
        "QTableCornerButton::section { background: #1a2238; border: none; }"
        // Buttons
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border: none; border-radius: 11px; min-height: 34px; padding: 2px 20px; font-weight: 700; font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4f7ef0, stop:1 #5457e6); }"
        "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4571e6, stop:1 #4a4ddb); }"
        "QPushButton:disabled { background: #dfe3ee; color: #9aa3b5; }"
        "QPushButton#secondaryButton { background: rgba(255,255,255,0.85); color: #4f46e5; border: 1px solid rgba(99,102,241,0.3); }"
        "QPushButton#secondaryButton:hover { background: #ffffff; border-color: #6366f1; }"
        "QPushButton#dangerButton { background: rgba(255,241,240,0.95); color: #b42318; border: 1px solid #ffd0cc; }"
        "QPushButton#dangerButton:hover { background: #ffe4e0; }"
        // Inputs
        "QLineEdit, QDateEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: rgba(255,255,255,0.95); border: 1px solid #d3d9e6; border-radius: 11px; min-height: 34px; padding: 0 12px; color: #1e2435; font-size: 13px; selection-background-color: #e0e7ff; selection-color: #1e2435; }"
        "QLineEdit:hover, QDateEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover, QComboBox:hover { border-color: #b6c0d4; }"
        "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus { border-color: #6366f1; background: #ffffff; }"
        "QLineEdit:disabled, QComboBox:disabled { background: #f1f4fa; color: #9aa3b5; }"
        "QComboBox::drop-down { border: none; width: 22px; }"
        "QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #d3d9e6; border-radius: 10px; selection-background-color: #e0e7ff; selection-color: #1e2435; outline: none; padding: 4px; }"
        "QDateEdit::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button, QSpinBox::up-button, QSpinBox::down-button { border: none; width: 18px; background: transparent; }"
        // Checkbox
        "QCheckBox { color: #1e2435; spacing: 8px; font-size: 13px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 6px; border: 1px solid #c2cad9; background: #ffffff; }"
        "QCheckBox::indicator:hover { border-color: #6366f1; }"
        "QCheckBox::indicator:checked { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); border: none; }"
        // Scrollbars
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: rgba(99,102,241,0.32); border-radius: 5px; min-height: 28px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(99,102,241,0.55); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: rgba(99,102,241,0.32); border-radius: 5px; min-width: 28px; }"
        "QScrollBar::handle:horizontal:hover { background: rgba(99,102,241,0.55); }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
        // Chrome
        "QStatusBar { background: rgba(255,255,255,0.7); color: #64748b; border-top: 1px solid rgba(148,163,184,0.25); font-size: 12px; }"
        "QStatusBar::item { border: none; }"
        "QDialog, QMessageBox { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #eef1fb, stop:1 #eae8fa); }"
        "QToolBar { background: rgba(255,255,255,0.65); border: none; border-bottom: 1px solid rgba(148,163,184,0.22); spacing: 10px; padding: 0 14px; min-height: 46px; }"
        "QToolBar QLabel { color: #1e2435; font-size: 13px; }"
        "QLabel#toolbarTitle { color: #161c2d; font-size: 14px; font-weight: 800; }"
        "QToolButton { background: transparent; color: #1e2435; border: none; padding: 6px 8px; border-radius: 8px; font-size: 13px; }"
        "QToolButton:hover { background: rgba(99,102,241,0.12); }"
        "QMenu { background: #ffffff; border: 1px solid #d3d9e6; border-radius: 10px; padding: 6px; }"
        "QMenu::item { padding: 7px 18px; border-radius: 7px; color: #1e2435; }"
        "QMenu::item:selected { background: #e0e7ff; }"
        "QSplitter::handle { background: rgba(99,102,241,0.18); }"
        // Welcome hero
        "QFrame#welcomeHero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #312e81, stop:0.45 #3730a3, stop:1 #2563eb); border-radius: 20px; border: 1px solid rgba(255,255,255,0.14); }"
        "QLabel#welcomeKicker { color: rgba(255,255,255,0.72); font-size: 11px; font-weight: 800; letter-spacing: 1.5px; background: transparent; }"
        "QLabel#welcomeTitle { color: #ffffff; font-size: 34px; font-weight: 800; letter-spacing: -0.5px; background: transparent; }"
        "QLabel#welcomeSubtitle { color: rgba(255,255,255,0.82); font-size: 13px; background: transparent; }"
        "QLabel#welcomeStat { color: #ffffff; background: rgba(255,255,255,0.14); border: 1px solid rgba(255,255,255,0.24); border-radius: 13px; padding: 6px 13px; font-size: 12px; font-weight: 700; }"
        "QPushButton#heroCta { background: #ffffff; color: #4f46e5; border: none; border-radius: 11px; min-height: 36px; padding: 4px 22px; font-weight: 800; font-size: 13px; }"
        "QPushButton#heroCta:hover { background: #eef0ff; }"
        "QPushButton#heroCta:pressed { background: #e2e5ff; }"
        "QPushButton#heroGhost { background: rgba(255,255,255,0.1); color: #ffffff; border: 1px solid rgba(255,255,255,0.32); border-radius: 11px; min-height: 36px; padding: 4px 22px; font-weight: 700; font-size: 13px; }"
        "QPushButton#heroGhost:hover { background: rgba(255,255,255,0.2); }"
        "QPushButton#heroGhost:pressed { background: rgba(255,255,255,0.28); }"
        // Feature cards & onboarding
        "QFrame#featureCard { background: rgba(255,255,255,0.7); border: 1px solid rgba(148,163,184,0.22); border-radius: 14px; }"
        "QFrame#featureCard:hover { background: #ffffff; border-color: rgba(99,102,241,0.4); }"
        "QLabel#featureIcon { color: #6366f1; font-size: 22px; }"
        "QLabel#featureTitle { color: #1e2435; font-size: 13px; font-weight: 700; }"
        "QLabel#featureDesc { color: #64748b; font-size: 12px; }"
        "QFrame#onboardStep { background: transparent; border: none; border-radius: 12px; }"
        "QFrame#onboardStep:hover { background: rgba(99,102,241,0.06); }"
        "QLabel#stepBadge { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border-radius: 14px; font-size: 12px; font-weight: 800; min-width: 28px; max-width: 28px; min-height: 28px; max-height: 28px; qproperty-alignment: AlignCenter; }"
        "QLabel#stepTitle { color: #1e2435; font-size: 14px; font-weight: 800; }"
        "QLabel#stepDesc { color: #64748b; font-size: 12px; }"
        "QLabel#formStatus { color: #64748b; font-size: 12px; font-weight: 700; }"
    );
    return cachedLight;
}

#if 0
static QString buildModernQssLegacy(bool darkMode) {
    Q_UNUSED(darkMode);
    const QString bg           = QStringLiteral("#f0f4f8");
    const QString surface      = QStringLiteral("#ffffff");
    const QString sidebarBg    = QStringLiteral("#1e293b");
    const QString sidebarSel   = QStringLiteral("#3b82f6");
    const QString sidebarSelTx = QStringLiteral("#ffffff");
    const QString sidebarHover = QStringLiteral("#334155");
    const QString textPrimary  = QStringLiteral("#1e293b");
    const QString textSecondary= QStringLiteral("#64748b");
    const QString border       = QStringLiteral("#e2e8f0");
    const QString inputBorder  = QStringLiteral("#cbd5e1");
    const QString tableHeaderBg= QStringLiteral("#1e293b");
    const QString tableHeaderTx= QStringLiteral("#f8fafc");
    const QString tableAltRow  = QStringLiteral("#f8fafc");
    const QString tableGrid    = QStringLiteral("#e2e8f0");
    const QString tableSelBg   = QStringLiteral("#dbeafe");
    const QString accent       = QStringLiteral("#3b82f6");
    const QString accentHover  = QStringLiteral("#2563eb");
    const QString accentPanelBg= QStringLiteral("#eff6ff");
    const QString inputBg      = QStringLiteral("#ffffff");
    const QString scrollHandle = QStringLiteral("#94a3b8");
    const QString secondaryBg  = QStringLiteral("#f1f5f9");
    const QString secondaryHover = QStringLiteral("#e2e8f0");
    const QString sidebarText  = QStringLiteral("#e2e8f0");
    const QString sidebarMuted = QStringLiteral("#94a3b8");
    const QString heroGrad     = QStringLiteral("qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1e40af, stop:1 #3b82f6)");

    return QStringLiteral(
        // 1. Universal font reset - normal weight baseline
        "* { font-family: 'Inter Tight', 'Segoe UI', '-apple-system', 'Helvetica Neue', sans-serif;"
        " font-weight: 400; }"

        // 2. Workspace
        "QMainWindow, QWidget#workspace { background: %1; color: %2; font-size: 13px; }"
    ).arg(bg, textPrimary)

    // 3. Sidebar container (dark)
    + QStringLiteral(
        "QWidget#sidebarWrap { background: %1; border-right: 1px solid rgba(0,0,0,0.2); }"
        "QWidget#sidebarWrapCollapsed { background: %2; border-right: 1px solid rgba(0,0,0,0.2); }"
    ).arg(sidebarBg, darkMode ? QStringLiteral("#151d2b") : QStringLiteral("#26312b"))

    + QStringLiteral(
        "QPushButton#sidebarToggle { background: rgba(255,255,255,0.08); color: %1;"
        " border: 1px solid rgba(255,255,255,0.18); border-radius: 7px; min-height: 28px;"
        " padding: 0 8px; font-weight: 800; }"
        "QPushButton#sidebarToggle:hover { background: rgba(255,255,255,0.16); }"
        "QPushButton#sidebarToggleCollapsed { background: %2; color: #ffffff;"
        " border: none; border-radius: 7px; min-height: 28px;"
        " padding: 0 8px; font-weight: 900; }"
        "QPushButton#sidebarToggleCollapsed:hover { background: %3; }"
    ).arg(sidebarText, accent, accentHover)

    // 4. Sidebar list (light text on dark bg)
    + QStringLiteral(
        "QListWidget#sidebar { background: transparent; border: none; color: %1;"
        " font-size: 13px; font-weight: 500; outline: none; padding: 8px 0; }"
    ).arg(sidebarText)

    // 5. Sidebar items
    + QStringLiteral(
        "QListWidget#sidebar::item { padding: 8px 10px; border-radius: 7px; min-height: 22px; margin: 1px 6px; color: %1; }"
    ).arg(sidebarText)

    // 6. Sidebar selected - vibrant blue with white bold text
    + QStringLiteral(
        "QListWidget#sidebar::item:selected { background: %1; color: %2; font-weight: 700; }"
    ).arg(sidebarSel, sidebarSelTx)

    // 7. Sidebar hover
    + QStringLiteral(
        "QListWidget#sidebar::item:hover:!selected { background: %1; }"
    ).arg(sidebarHover)

    // 8. Brand label (on dark sidebar)
    + QStringLiteral(
        "QLabel#brandLabel { color: %1; font-size: 15px; font-weight: 700;"
        " padding: 11px 14px 10px 14px; }"
    ).arg(sidebarText)

    // 9. Brand sub
    + QStringLiteral(
        "QLabel#brandSub { color: %1; font-size: 11px; padding: 0 14px 10px 14px; }"
    ).arg(sidebarMuted)

    // 10. Page title
    + QStringLiteral(
        "QLabel#pageTitle { color: %1; font-size: 20px; font-weight: 700; letter-spacing: -0.5px; }"
    ).arg(textPrimary)

    // 11. Page meta
    + QStringLiteral(
        "QLabel#pageMeta { color: %1; font-size: 12px; }"
    ).arg(textSecondary)

    // 12. Section title
    + QStringLiteral(
        "QLabel#sectionTitle { color: %1; font-size: 15px; font-weight: 800; }"
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

    // 15. Panel - no border, just white space
    + QStringLiteral(
        "QFrame#panel { background: %1; border: 1px solid %2; border-radius: 12px; }"
    ).arg(surface, border)

    // 15b. Form panel - visible border
    + QStringLiteral(
        "QFrame#formPanel { background: %1; border: 1px solid %2; border-radius: 12px; }"
    ).arg(surface, border)

    // 16. Accent panel
    + QStringLiteral(
        "QFrame#accentPanel { background: %1; border: none; border-radius: 6px; }"
    ).arg(accentPanelBg)

    // 17. Context bar - transparent, no border
    + QStringLiteral(
        "QFrame#contextBar { background: transparent; border: none; padding: 0; }"
    )

    // 18. Tables - flat, clean, soft-blue selection, no vertical grid
    + QStringLiteral(
        "QTableWidget#dataTable, QTableWidget#inventoryTable {"
        " background: %1; alternate-background-color: %2;"
        " border: none; border-radius: 0px;"
        " selection-background-color: %3; selection-color: %4;"
        " gridline-color: %5; color: %4; font-size: 13px; font-weight: 500;"
        " outline: none; }"
    ).arg(surface, tableAltRow, tableSelBg, textPrimary, tableGrid)

    // 19. Header - flat white, small uppercase muted text, bottom border only
    + QStringLiteral(
        "QHeaderView::section {"
        " background-color: %1; color: %2;"
        " border: none; border-bottom: 1px solid %3;"
        " padding: 8px 10px; font-weight: 800; font-size: 12px;"
        " letter-spacing: 0px; }"
    ).arg(tableHeaderBg, tableHeaderTx, border)

    // 20. Header background fill
    + QStringLiteral(
        "QHeaderView { background-color: %1; }"
    ).arg(tableHeaderBg)

    // 21. Table items - tight padding, thin bottom border
    + QStringLiteral(
        "QTableWidget::item { padding: 6px 10px; border-bottom: 1px solid %1; color: %2; }"
        "QTableWidget#inventoryTable::item { padding: 4px 6px; border-right: 1px solid %1; }"
    ).arg(tableGrid, textPrimary)

    // 22. Primary button - solid blue, white bold text
    + QStringLiteral(
        "QPushButton { background: %1; color: #ffffff; border: none;"
        " border-radius: 10px; min-height: 32px; padding: 2px 18px;"
        " font-weight: 700; font-size: 13px; }"
    ).arg(accent)

    // 23. Button hover
    + QStringLiteral(
        "QPushButton:hover { background: %1; }"
        "QPushButton:pressed { background: %1; }"
    ).arg(accentHover)

    // 24. Secondary button - grey fill, no border (Clear / Delete)
    + QStringLiteral(
        "QPushButton#secondaryButton { background: %1; color: %2;"
        " border: none; }"
        "QPushButton#secondaryButton:hover { background: %3; }"
        "QPushButton#dangerButton { background: #fff1f0; color: #b42318; border: 1px solid #ffd0cc; }"
        "QPushButton#dangerButton:hover { background: #ffe4e0; }"
        "QPushButton:disabled { background: #edf0f3; color: #9aa3ad; }"
    ).arg(secondaryBg, textPrimary, secondaryHover)

    // 25. Inputs - 32px height, semi-rounded
    + QStringLiteral(
        "QLineEdit, QDateEdit, QDoubleSpinBox, QComboBox {"
        " background: %1; border: 1px solid %2; border-radius: 10px;"
        " min-height: 32px; padding: 0 12px; color: %3; font-size: 13px; }"
    ).arg(inputBg, inputBorder, textPrimary)

    // 26. Focus states
    + QStringLiteral(
        "QLineEdit:focus, QDateEdit:focus, QDoubleSpinBox:focus, QComboBox:focus {"
        " border-color: %1; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
    ).arg(accent)

    // 27. Scrollbars - 8px, subtle
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
        "QToolBar { background: %1; border-bottom: 1px solid %2; spacing: 10px; padding: 0 14px; min-height: 44px; }"
        "QToolBar QLabel { color: %3; font-size: 13px; }"
        "QLabel#toolbarTitle { color: %3; font-size: 14px; font-weight: 700; }"
        "QToolButton { background: transparent; color: %3; border: none; padding: 6px 4px; font-size: 13px; }"
        "QToolButton:hover { background: rgba(0,0,0,0.05); border-radius: 7px; }"
    ).arg(surface, border, textPrimary)

    // 30. Welcome hero - accent blue gradient
    + QStringLiteral(
        "QFrame#welcomeHero {"
        " background: %1;"
        " border-radius: 14px; border: none; }"
    ).arg(heroGrad)

    + QStringLiteral(
        "QLabel#welcomeKicker {"
        " color: rgba(255,255,255,0.72); font-size: 11px; font-weight: 700; letter-spacing: 1px; }"

        "QLabel#welcomeTitle {"
        " color: #ffffff; font-size: 32px; font-weight: 800; }"

        "QLabel#welcomeSubtitle {"
        " color: rgba(255,255,255,0.8); font-size: 13px; }"
    )

    // 31. Welcome stat chips - subtle bg
    + QStringLiteral(
        "QLabel#welcomeStat {"
        " color: #ffffff; background: rgba(255,255,255,0.15);"
        " border: 1px solid rgba(255,255,255,0.2);"
        " border-radius: 14px; padding: 6px 12px;"
        " font-size: 12px; font-weight: 700; }"
    )

    // 32. Feature cards - clean, no border
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

    // 33. Onboarding steps - clean, minimal
    + QStringLiteral(
        "QFrame#onboardStep { background: transparent; border: none; border-radius: 8px; }"
    ).arg(surface)

    + QStringLiteral(
        "QLabel#stepBadge {"
        " background: %1; color: #ffffff; border-radius: 11px;"
        " font-size: 11px; font-weight: 600;"
        " min-width: 22px; max-width: 22px; min-height: 22px; max-height: 22px;"
        " qproperty-alignment: AlignCenter; }"
    ).arg(accent)

    + QStringLiteral(
        "QLabel#stepTitle { color: %1; font-size: 14px; font-weight: 800; }"
        "QLabel#stepDesc { color: %2; font-size: 12px; }"
    ).arg(textPrimary, textSecondary)

    + QStringLiteral(
        "QFrame#divider { background: %1; border: none; max-height: 1px; }"
    ).arg(border)

    + QStringLiteral(
        "QPushButton#heroCta {"
        " background: #ffffff; color: %1; border: none; border-radius: 6px;"
        " min-height: 32px; padding: 4px 20px; font-weight: 800; font-size: 13px; }"
        "QPushButton#heroCta:hover { background: #f0f0f0; }"
        "QPushButton#heroCta:pressed { background: #e0e0e0; }"
    ).arg(accent)

    + QStringLiteral(
        "QPushButton#heroGhost {"
        " background: transparent; color: #ffffff;"
        " border: 1px solid rgba(255,255,255,0.25); border-radius: 6px;"
        " min-height: 32px; padding: 4px 20px; font-weight: 700; font-size: 13px; }"
        "QPushButton#heroGhost:hover { background: rgba(255,255,255,0.18); }"
        "QPushButton#heroGhost:pressed { background: rgba(255,255,255,0.25); }"
        "QLabel#formStatus { color: #64748b; font-size: 12px; font-weight: 700; }"
    );
}
#endif

bool systemPrefersDarkTheme() {
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QStringList monthNames() {
    return {
        QStringLiteral("01 - January"), QStringLiteral("02 - February"), QStringLiteral("03 - March"),
        QStringLiteral("04 - April"), QStringLiteral("05 - May"), QStringLiteral("06 - June"),
        QStringLiteral("07 - July"), QStringLiteral("08 - August"), QStringLiteral("09 - September"),
        QStringLiteral("10 - October"), QStringLiteral("11 - November"), QStringLiteral("12 - December")
    };
}

QString inventoryMailProfilePath() {
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.trimmed().isEmpty()) {
        throw std::runtime_error("Could not resolve app data path for inventory mail profile");
    }
    QDir appDataDir(appDataPath);
    if (!appDataDir.exists() && !appDataDir.mkpath(QStringLiteral("."))) {
        throw std::runtime_error(QStringLiteral("Could not create app data path for inventory mail profile: %1").arg(appDataPath).toStdString());
    }
    return appDataDir.filePath(QStringLiteral("inventory_mail_profile.json"));
}

std::optional<InventoryMailProfile> readInventoryMailProfile() {
    const QString path = inventoryMailProfilePath();
    QFile file(path);
    if (!file.exists()) {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(QStringLiteral("Could not read inventory mail profile at %1: %2").arg(path, file.errorString()).toStdString());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error(QStringLiteral("Inventory mail profile is invalid JSON at %1: %2").arg(path, parseError.errorString()).toStdString());
    }

    const QJsonObject payload = document.object();
    return InventoryMailProfile{
        payload.value(QStringLiteral("to_email")).toString(),
        payload.value(QStringLiteral("cc_email")).toString(),
        payload.value(QStringLiteral("sender_email")).toString(),
        payload.value(QStringLiteral("smtp_host")).toString(QStringLiteral("smtp.gmail.com")),
        payload.value(QStringLiteral("smtp_port")).toString(QStringLiteral("587")),
        payload.value(QStringLiteral("subject")).toString(QStringLiteral("Inventory Report")),
        payload.value(QStringLiteral("average_mode")).toString(QStringLiteral("monthly"))
    };
}

void writeInventoryMailProfile(const InventoryMailProfile& profile) {
    const QString path = inventoryMailProfilePath();
    QJsonObject payload;
    payload.insert(QStringLiteral("to_email"), profile.toEmail);
    payload.insert(QStringLiteral("cc_email"), profile.ccEmail);
    payload.insert(QStringLiteral("sender_email"), profile.senderEmail);
    payload.insert(QStringLiteral("smtp_host"), profile.smtpHost);
    payload.insert(QStringLiteral("smtp_port"), profile.smtpPort);
    payload.insert(QStringLiteral("subject"), profile.subject);
    payload.insert(QStringLiteral("average_mode"), profile.averageMode);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("Could not write inventory mail profile at %1: %2").arg(path, file.errorString()).toStdString());
    }
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
}

QVector<mfinlogs::domain::InventoryProductRow> inventoryProductRowsFromJson(const QJsonArray& sourceRows) {
    QVector<mfinlogs::domain::InventoryProductRow> rows;
    rows.reserve(sourceRows.size());
    for (const QJsonValue& value : sourceRows) {
        const QJsonObject item = value.toObject();
        QVector<double> quantities;
        QVector<double> purchases;
        quantities.reserve(31);
        purchases.reserve(31);
        for (int day = 1; day <= 31; day += 1) {
            const QString dayText = QString::number(day).rightJustified(2, QLatin1Char('0'));
            quantities.append(item.value(QStringLiteral("qty_%1").arg(dayText)).toDouble());
            purchases.append(item.value(QStringLiteral("purchase_%1").arg(dayText)).toDouble());
        }
        rows.append(mfinlogs::domain::InventoryProductRow{
            item.value(QStringLiteral("name")).toString(),
            item.value(QStringLiteral("cost")).toDouble(),
            item.value(QStringLiteral("min_stock")).toDouble(),
            quantities,
            purchases
        });
    }
    return rows;
}

} // namespace

namespace mfinlogs::app {

DesktopApplication::DesktopApplication(AppContext& context)
    : context_(context) {
    setWindowTitle(QStringLiteral("M-Finlogs Native"));
    resize(1360, 860);
    applyTheme(systemPrefersDarkTheme());
    buildNavigation();
    wireActions();
    if (!showAuthDialog()) {
        QTimer::singleShot(0, qApp, &QApplication::quit);
    }
}

void DesktopApplication::applyTheme(bool darkMode) {
    // Try multiple font paths: next to exe, project root, or parent directory
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList fontSearchPaths = {
        appDir + QStringLiteral("/fonts/"),
        appDir + QStringLiteral("/../fonts/"),
        appDir + QStringLiteral("/../../fonts/"),
        appDir + QStringLiteral("/../../../fonts/"),
    };
    for (const QString& fontDir : fontSearchPaths) {
        if (QDir(fontDir).exists()) {
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("InterTight-Bold.ttf"));
            // Space Mono is used for the inventory PDF report (monospace layout)
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Regular.ttf"));
            QFontDatabase::addApplicationFont(fontDir + QStringLiteral("SpaceMono-Bold.ttf"));
            break;
        }
    }
    qApp->setFont(QFont(QStringLiteral("Inter Tight"), 10));
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
    sidebarTopInset->setFixedHeight(54);
    QHBoxLayout* sidebarTopLayout = new QHBoxLayout(sidebarTopInset);
    sidebarTopLayout->setContentsMargins(12, 10, 8, 8);
    sidebarTopLayout->setSpacing(8);
    QLabel* sidebarBrand = new QLabel(QStringLiteral("M-Finlogs"), sidebarTopInset);
    sidebarBrand->setObjectName(QStringLiteral("brandLabel"));
    QPushButton* sidebarToggle = new QPushButton(QStringLiteral("<"), sidebarTopInset);
    sidebarToggle->setObjectName(QStringLiteral("sidebarToggle"));
    sidebarToggle->setFixedWidth(34);
    sidebarToggle->setToolTip(QStringLiteral("Collapse sidebar"));
    sidebarTopLayout->addWidget(sidebarBrand, 1);
    sidebarTopLayout->addWidget(sidebarToggle);
    sidebarLayout->addWidget(sidebarTopInset);

    QListWidget* nav = new QListWidget(sidebar);
    nav->setObjectName(QStringLiteral("sidebar"));
    nav->setIconSize(QSize(18, 18));

    QStackedWidget* pages = new QStackedWidget(root);
    pages_ = pages;
    int pageIndex = 0;

    // Build Welcome page eagerly (initial page)
    pages->addWidget(buildWelcomePage(*pages, *nav));
    addNavItem(*nav, QStringLiteral("Welcome"), pageIndex, QStringLiteral("welcome"));
    pageIndex += 1;

    // Register factories for all other pages - built on first visit
    auto addLazyPage = [&](auto factory, const QString& label, const QString& icon) {
        pages->addWidget(new QWidget());
        pageFactories_.append(factory);
        addNavItem(*nav, label, pageIndex, icon);
        pageIndex += 1;
    };

    addLazyPage([this]() { return buildDailyEntryPage(); },        QStringLiteral("Daily Entry"),    QStringLiteral("entry"));
    addLazyPage([this]() { return buildDashboardPage(); },         QStringLiteral("Dashboard"),     QStringLiteral("dashboard"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Reports"),             QStringLiteral("Report overview and transaction analysis.")); },
                                                                     QStringLiteral("Reports"),       QStringLiteral("reports"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Party Ledger"),        QStringLiteral("Party ledger with date filters and running balances.")); },
                                                                     QStringLiteral("Party Ledger"),  QStringLiteral("ledger"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Day Book"),            QStringLiteral("Daily transaction register for selected dates.")); },
                                                                     QStringLiteral("Day Book"),      QStringLiteral("daybook"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Daily Summary"),       QStringLiteral("Daily sales, returns, receipts, and expenses.")); },
                                                                     QStringLiteral("Daily Summary"), QStringLiteral("summary"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Short / Excess"),      QStringLiteral("Cash-in-hand snapshots by day.")); },
                                                                     QStringLiteral("Short / Excess"),QStringLiteral("cash"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Purchase Report"),     QStringLiteral("Purchase summary and supplier analysis.")); },
                                                                     QStringLiteral("Purchase Report"),QStringLiteral("reports"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Expenses"),            QStringLiteral("Expense transactions and totals.")); },
                                                                     QStringLiteral("Expenses"),      QStringLiteral("expenses"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Outstanding"),         QStringLiteral("Customer balances for the selected financial year.")); },
                                                                     QStringLiteral("Outstanding"),   QStringLiteral("outstanding"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Trial Balance"),       QStringLiteral("Account debit and credit balances.")); },
                                                                     QStringLiteral("Trial Balance"), QStringLiteral("trend"));
    addLazyPage([this]() { return buildReportPage(QStringLiteral("Profit & Loss"),       QStringLiteral("Sales, expenses, and net profit summary.")); },
                                                                     QStringLiteral("P & L"),         QStringLiteral("trend"));
    addLazyPage([this]() { return buildInventoryPage(); },          QStringLiteral("Inventory"),      QStringLiteral("inventory"));
    addLazyPage([this]() { return buildInventoryValuePage(); },     QStringLiteral("Stock Value Report"), QStringLiteral("stock"));
    addLazyPage([this]() { return buildPartiesPage(); },            QStringLiteral("Add Party"),       QStringLiteral("party"));
    addLazyPage([this]() { return buildAuditPage(); },              QStringLiteral("Audit Logs"),      QStringLiteral("audit"));
    addLazyPage([this]() { return buildSettingsPage(); },           QStringLiteral("Settings"),        QStringLiteral("settings"));
    sidebarLayout->addWidget(nav, 1);

    const std::shared_ptr<bool> sidebarCollapsed = std::make_shared<bool>(false);
    auto applySidebarState = [sidebar, nav, sidebarBrand, sidebarToggle, sidebarCollapsed]() {
        sidebar->setFixedWidth(*sidebarCollapsed ? 56 : 240);
        sidebar->setObjectName(*sidebarCollapsed ? QStringLiteral("sidebarWrapCollapsed") : QStringLiteral("sidebarWrap"));
        sidebar->style()->unpolish(sidebar);
        sidebar->style()->polish(sidebar);
        sidebarBrand->setVisible(!*sidebarCollapsed);
        sidebarToggle->setObjectName(*sidebarCollapsed ? QStringLiteral("sidebarToggleCollapsed") : QStringLiteral("sidebarToggle"));
        sidebarToggle->style()->unpolish(sidebarToggle);
        sidebarToggle->style()->polish(sidebarToggle);
        sidebarToggle->setText(*sidebarCollapsed ? QStringLiteral("\xe2\x96\xb6") : QStringLiteral("\xe2\x97\x80"));
        sidebarToggle->setToolTip(*sidebarCollapsed ? QStringLiteral("Expand sidebar") : QStringLiteral("Collapse sidebar"));
        sidebarToggle->setFixedWidth(*sidebarCollapsed ? 40 : 34);
        // Larger icons when collapsed for better visibility
        nav->setIconSize(*sidebarCollapsed ? QSize(22, 22) : QSize(18, 18));
        for (int row = 0; row < nav->count(); row += 1) {
            QListWidgetItem* item = nav->item(row);
            const int pageIndex = item->data(Qt::UserRole).toInt();
            const QString label = item->data(Qt::UserRole + 1).toString();
            // Hide group headers when collapsed
            if (pageIndex < 0) {
                item->setHidden(*sidebarCollapsed);
                continue;
            }
            if (!label.isEmpty()) {
                item->setText(*sidebarCollapsed ? QString() : label);
                item->setSizeHint(*sidebarCollapsed ? QSize(40, 36) : QSize(-1, -1));
            }
        }
    };
    connect(sidebarToggle, &QPushButton::clicked, this, [sidebarCollapsed, applySidebarState]() {
        *sidebarCollapsed = !*sidebarCollapsed;
        applySidebarState();
    });

    connect(nav, &QListWidget::currentRowChanged, this, [this, nav, pages](int row) {
        const QListWidgetItem* item = nav->item(row);
        if (!item) {
            return;
        }
        const int targetPage = item->data(Qt::UserRole).toInt();
        if (targetPage >= 0) {
            QWidget* page = ensurePageBuilt(targetPage);
            if (page) {
                pages->setCurrentIndex(targetPage);
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
        dialog.setWindowTitle(setupRequired ? QStringLiteral("Create Admin Password") : QStringLiteral("M-Finlogs - Sign In"));
        dialog.setModal(true);
        // Premium glass sign-in styling with hover/focus states
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #f3f5fd, stop:1 #eceaf9); }"
            "QLabel { background: transparent; }"
            "QLineEdit { background: rgba(255,255,255,0.92); border: 1.5px solid #dbe1ee; border-radius: 11px;"
            " padding: 0 14px; color: #1e293b; font-size: 13px; }"
            "QLineEdit:hover { border-color: #aeb9e8; background: #ffffff; }"
            "QLineEdit:focus { border-color: #6366f1; background: #ffffff; }"
            "QLineEdit:disabled { background: #f1f5f9; color: #94a3b8; }"
            "QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #5b8cfa, stop:1 #6366f1); color: #ffffff; border: none; border-radius: 11px;"
            " font-weight: 700; font-size: 14px; padding: 8px 18px; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4f7ef0, stop:1 #5457e6); }"
            "QPushButton:pressed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #4571e6, stop:1 #4a4ddb); }"
            "QPushButton#secondaryButton { background: rgba(255,255,255,0.85); color: #4f46e5; font-weight: 600; border: 1px solid rgba(99,102,241,0.3); border-radius: 11px; }"
            "QPushButton#secondaryButton:hover { background: #ffffff; border-color: #6366f1; }"
        ));
        dialog.resize(440, 392);

        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(38, 34, 38, 30);
        layout->setSpacing(0);

        // Logo / Brand section
        QLabel* logoLabel = new QLabel(QStringLiteral("M"), &dialog);
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setFixedSize(56, 56);
        logoLabel->setStyleSheet(QStringLiteral(
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #5b8cfa, stop:1 #6366f1);"
            "border-radius: 16px; color: #ffffff; font-size: 24px; font-weight: 800;"));
        QHBoxLayout* logoRow = new QHBoxLayout();
        logoRow->addStretch(1);
        logoRow->addWidget(logoLabel);
        logoRow->addStretch(1);
        layout->addLayout(logoRow);
        layout->addSpacing(18);

        QLabel* title = new QLabel(setupRequired ? QStringLiteral("Create Admin Password") : QStringLiteral("Welcome Back"), &dialog);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: 700; color: #1e293b;"));
        layout->addWidget(title);
        layout->addSpacing(4);

        QLabel* subtitle = new QLabel(setupRequired
            ? QStringLiteral("Set up the admin password for this database")
            : QStringLiteral("Sign in to manage your accounting workspace"), &dialog);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet(QStringLiteral("font-size: 12px; color: #64748b;"));
        layout->addWidget(subtitle);
        layout->addSpacing(24);

        // Input fields
        QLineEdit* username = new QLineEdit(setupRequired ? QStringLiteral("admin") : QString(), &dialog);
        username->setPlaceholderText(QStringLiteral("Username"));
        username->setEnabled(!setupRequired);
        username->setMinimumHeight(38);
        QLineEdit* password = new QLineEdit(&dialog);
        password->setPlaceholderText(setupRequired ? QStringLiteral("Create password") : QStringLiteral("Password"));
        password->setEchoMode(QLineEdit::Password);
        password->setMinimumHeight(38);
        layout->addWidget(username);
        layout->addSpacing(10);
        layout->addWidget(password);
        layout->addSpacing(8);

        QLabel* error = new QLabel(&dialog);
        error->setStyleSheet(QStringLiteral("color: #EF4444; font-size: 11px; font-weight: 500;"));
        error->setAlignment(Qt::AlignCenter);
        layout->addWidget(error);
        layout->addSpacing(16);

        QPushButton* submit = new QPushButton(setupRequired ? QStringLiteral("Save Admin") : QStringLiteral("Sign In"), &dialog);
        submit->setMinimumHeight(40);
        layout->addWidget(submit);
        layout->addSpacing(12);

        QHBoxLayout* footerRow = new QHBoxLayout();
        QPushButton* configure = new QPushButton(QStringLiteral("Configure Database"), &dialog);
        configure->setObjectName(QStringLiteral("secondaryButton"));
        QPushButton* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
        cancel->setObjectName(QStringLiteral("secondaryButton"));
        footerRow->addWidget(configure);
        footerRow->addStretch(1);
        footerRow->addWidget(cancel);
        layout->addLayout(footerRow);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(configure, &QPushButton::clicked, this, [this, &dialog]() { showDatabaseConfigDialog(&dialog); });
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

void DesktopApplication::showDatabaseConfigDialog(QWidget* parent) {
    domain::DatabaseConfig config{};
    try {
        config = context_.services().config->readDatabaseConfig();
    } catch (const std::exception& err) {
        QMessageBox::critical(parent ? parent : this,
            QStringLiteral("Database Configuration"),
            QStringLiteral("Could not read database configuration.\n\n%1").arg(QString::fromUtf8(err.what())));
        return;
    }

    QDialog dialog(parent ? parent : this);
    dialog.setWindowTitle(QStringLiteral("Database Configuration"));
    dialog.setModal(true);
    dialog.resize(980, 620);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(14);
    QLabel* title = new QLabel(QStringLiteral("Database Configuration"), &dialog);
    QLabel* subtitle = new QLabel(QStringLiteral("Configure SQL Server connection, backup, restore, and native server controls."), &dialog);
    title->setObjectName(QStringLiteral("pageTitle"));
    subtitle->setObjectName(QStringLiteral("pageMeta"));
    layout->addWidget(title);
    layout->addWidget(subtitle);

    QFrame* panel = createPanel(&dialog);
    QGridLayout* form = new QGridLayout(panel);
    form->setContentsMargins(18, 16, 18, 16);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
    QLineEdit* server = new QLineEdit(config.server, panel);
    QLineEdit* apiBase = new QLineEdit(config.apiBaseUrl, panel);
    QLineEdit* database = new QLineEdit(config.database, panel);
    QComboBox* authType = new QComboBox(panel);
    authType->addItems({QStringLiteral("Windows Authentication"), QStringLiteral("SQL Server Authentication")});
    authType->setCurrentIndex(config.useWindowsAuth ? 0 : 1);
    QLineEdit* username = new QLineEdit(config.username, panel);
    QLineEdit* password = new QLineEdit(config.password, panel);
    QLineEdit* backupDir = new QLineEdit(config.backupDir, panel);
    password->setEchoMode(QLineEdit::Password);
    QLabel* status = new QLabel(panel);
    status->setObjectName(QStringLiteral("formStatus"));

    form->addWidget(new QLabel(QStringLiteral("SQL Server Instance"), panel), 0, 0);
    form->addWidget(server, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("API Server URL"), panel), 0, 1);
    form->addWidget(apiBase, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Database Name"), panel), 2, 0);
    form->addWidget(database, 3, 0);
    form->addWidget(new QLabel(QStringLiteral("Authentication Type"), panel), 2, 1);
    form->addWidget(authType, 3, 1);
    form->addWidget(new QLabel(QStringLiteral("SQL Username"), panel), 4, 0);
    form->addWidget(username, 5, 0);
    form->addWidget(new QLabel(QStringLiteral("SQL Password"), panel), 4, 1);
    form->addWidget(password, 5, 1);
    form->addWidget(new QLabel(QStringLiteral("Backup Target Folder"), panel), 6, 0, 1, 2);
    form->addWidget(backupDir, 7, 0, 1, 2);
    form->addWidget(status, 8, 0, 1, 2);
    layout->addWidget(panel);

    QWidget* actions = new QWidget(&dialog);
    QGridLayout* actionLayout = new QGridLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(10);
    QPushButton* test = new QPushButton(QStringLiteral("Test Connection"), actions);
    QPushButton* save = new QPushButton(QStringLiteral("Save & Apply"), actions);
    QPushButton* backup = new QPushButton(QStringLiteral("Backup Database"), actions);
    QPushButton* restore = new QPushButton(QStringLiteral("Choose Backup & Restore"), actions);
    QPushButton* startServer = new QPushButton(QStringLiteral("Start Native Server"), actions);
    QPushButton* stopServer = new QPushButton(QStringLiteral("Stop Native Server"), actions);
    QPushButton* close = new QPushButton(QStringLiteral("Close"), actions);
    backup->setObjectName(QStringLiteral("secondaryButton"));
    restore->setObjectName(QStringLiteral("secondaryButton"));
    startServer->setObjectName(QStringLiteral("secondaryButton"));
    stopServer->setObjectName(QStringLiteral("secondaryButton"));
    close->setObjectName(QStringLiteral("secondaryButton"));
    actionLayout->addWidget(test, 0, 0);
    actionLayout->addWidget(save, 0, 1);
    actionLayout->addWidget(backup, 0, 2);
    actionLayout->addWidget(restore, 0, 3);
    actionLayout->addWidget(startServer, 1, 0);
    actionLayout->addWidget(stopServer, 1, 1);
    actionLayout->addWidget(close, 1, 3);
    layout->addWidget(actions);

    auto buildConfig = [server, database, authType, username, password, backupDir, apiBase, config]() -> domain::DatabaseConfig {
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

    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(test, &QPushButton::clicked, this, [this, buildConfig, status]() {
        try {
            context_.services().config->testDatabaseConfig(buildConfig());
            status->setText(QStringLiteral("Connection successful"));
        } catch (const std::exception& err) {
            status->setText(QString::fromUtf8(err.what()));
        }
    });
    connect(save, &QPushButton::clicked, this, [this, buildConfig, status]() {
        try {
            const domain::DatabaseConfig nextConfig = buildConfig();
            context_.services().config->testDatabaseConfig(nextConfig);
            context_.services().config->writeDatabaseConfig(nextConfig);
            status->setText(QStringLiteral("Database configuration saved. Restart native app to reload all screens."));
        } catch (const std::exception& err) {
            status->setText(QString::fromUtf8(err.what()));
        }
    });
    connect(backup, &QPushButton::clicked, this, [this, backupDir, status]() {
        try {
            const domain::BackupResult result = context_.services().backups->backup(backupDir->text().trimmed());
            status->setText(result.status + QStringLiteral(": ") + result.path);
        } catch (const std::exception& err) {
            status->setText(QString::fromUtf8(err.what()));
        }
    });
    connect(restore, &QPushButton::clicked, this, [this, backupDir, status]() {
        const QString backupPath = QFileDialog::getOpenFileName(this, QStringLiteral("Choose SQL Backup"), backupDir->text().trimmed(), QStringLiteral("SQL Backup (*.bak)"));
        if (backupPath.trimmed().isEmpty()) {
            return;
        }
        try {
            context_.services().backups->restore(backupPath);
            status->setText(QStringLiteral("Backup restored. Restart native app to reload all screens."));
        } catch (const std::exception& err) {
            status->setText(QString::fromUtf8(err.what()));
        }
    });
    connect(startServer, &QPushButton::clicked, this, [status]() {
        status->setText(QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--server-start")})
            ? QStringLiteral("Native server start requested")
            : QStringLiteral("Could not start native server"));
    });
    connect(stopServer, &QPushButton::clicked, this, [status]() {
        status->setText(QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--server-stop")})
            ? QStringLiteral("Native server stop requested")
            : QStringLiteral("Could not stop native server"));
    });

    dialog.exec();
}

QWidget* DesktopApplication::buildWelcomePage(QStackedWidget& pages, QListWidget& nav) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 20);
    layout->setSpacing(16);

    QFrame* hero = new QFrame(page);
    hero->setObjectName(QStringLiteral("welcomeHero"));
    hero->setMinimumHeight(210);
    QVBoxLayout* heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(30, 26, 30, 26);
    heroLayout->setSpacing(0);

    QLabel* kicker = new QLabel(QStringLiteral("M-FINLOGS - NATIVE ACCOUNTING WORKSPACE"), hero);
    QLabel* headline = new QLabel(QStringLiteral("Welcome back."), hero);
    QLabel* subline = new QLabel(currentCompanyText(context_) + QStringLiteral(" - ") + currentFinancialYearText(), hero);
    kicker->setObjectName(QStringLiteral("welcomeKicker"));
    headline->setObjectName(QStringLiteral("welcomeTitle"));
    subline->setObjectName(QStringLiteral("welcomeSubtitle"));

    QWidget* chipsRow = new QWidget(hero);
    QHBoxLayout* chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 18, 0, 0);
    chipsLayout->setSpacing(10);
    const QStringList chips = {
        QStringLiteral("Keyboard-first entry"),
        QStringLiteral("SQL Server backed"),
        QStringLiteral("Native runtime"),
        QStringLiteral("Live reports")
    };
    for (const QString& chip : chips) {
        QLabel* label = new QLabel(chip, chipsRow);
        label->setObjectName(QStringLiteral("welcomeStat"));
        chipsLayout->addWidget(label);
    }
    chipsLayout->addStretch(1);

    QWidget* ctaRow = new QWidget(hero);
    QHBoxLayout* ctaLayout = new QHBoxLayout(ctaRow);
    ctaLayout->setContentsMargins(0, 22, 0, 0);
    ctaLayout->setSpacing(12);
    QPushButton* ctaEntry = new QPushButton(QStringLiteral("Open Daily Entry"), hero);
    QPushButton* ctaReports = new QPushButton(QStringLiteral("View Reports"), hero);
    QPushButton* ctaInv = new QPushButton(QStringLiteral("Inventory"), hero);
    QPushButton* ctaSettings = new QPushButton(QStringLiteral("Settings"), hero);
    ctaEntry->setObjectName(QStringLiteral("heroCta"));
    ctaReports->setObjectName(QStringLiteral("heroGhost"));
    ctaInv->setObjectName(QStringLiteral("heroGhost"));
    ctaSettings->setObjectName(QStringLiteral("heroGhost"));
    ctaEntry->setIcon(appIcon(QStringLiteral("entry")));
    ctaReports->setIcon(appIcon(QStringLiteral("reports")));
    ctaInv->setIcon(appIcon(QStringLiteral("inventory")));
    ctaSettings->setIcon(appIcon(QStringLiteral("settings")));
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
          QStringLiteral("Go to Settings, enter your server, database, username and password, then click Save.") },
        { QStringLiteral("2"), QStringLiteral("Add your parties"),
          QStringLiteral("Open Add Party to create customer and supplier names before entering transactions.") },
        { QStringLiteral("3"), QStringLiteral("Enter daily transactions"),
          QStringLiteral("Use Daily Entry for sales, purchases, receipts and expenses. Tab / Enter moves between fields.") },
        { QStringLiteral("4"), QStringLiteral("Review reports"),
          QStringLiteral("Party Ledger, Day Book, P&L and Trial Balance update instantly from your entries.") },
    };

    for (int stepIndex = 0; stepIndex < steps.size(); stepIndex += 1) {
        const StepDef& step = steps[stepIndex];
        QFrame* stepRow = new QFrame(onboard);
        stepRow->setObjectName(QStringLiteral("onboardStep"));
        QHBoxLayout* stepLayout = new QHBoxLayout(stepRow);
        stepLayout->setContentsMargins(16, 12, 16, 12);
        stepLayout->setSpacing(16);

        QLabel* badge = new QLabel(step.num, stepRow);
        badge->setObjectName(QStringLiteral("stepBadge"));
        badge->setFixedSize(28, 28);
        badge->setAlignment(Qt::AlignCenter);

        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(3);
        QLabel* stepTitle = new QLabel(step.title, stepRow);
        QLabel* stepDesc = new QLabel(step.desc, stepRow);
        stepTitle->setObjectName(QStringLiteral("stepTitle"));
        stepDesc->setObjectName(QStringLiteral("stepDesc"));
        stepDesc->setWordWrap(true);
        textLayout->addWidget(stepTitle);
        textLayout->addWidget(stepDesc);

        stepLayout->addWidget(badge);
        stepLayout->addLayout(textLayout, 1);
        onboardLayout->addWidget(stepRow);

        if (stepIndex < steps.size() - 1) {
            QFrame* divider = new QFrame(onboard);
            divider->setObjectName(QStringLiteral("divider"));
            divider->setFrameShape(QFrame::HLine);
            divider->setFixedHeight(1);
            onboardLayout->addWidget(divider);
        }
    }
    layout->addWidget(onboard);

    QLabel* tipLabel = new QLabel(QStringLiteral("Press Ctrl+1..9 to jump between pages"), page);
    tipLabel->setObjectName(QStringLiteral("pageMeta"));
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);
    layout->addStretch(1);

    QPointer<QStackedWidget> pagesPtr(&pages);
    QPointer<QListWidget> navPtr(&nav);
    auto goToPage = [pagesPtr, navPtr](const QString& label) {
        if (!pagesPtr || !navPtr) {
            return;
        }
        for (int row = 0; row < navPtr->count(); row += 1) {
            QListWidgetItem* item = navPtr->item(row);
            if (item && item->data(Qt::UserRole + 1).toString() == label) {
                navPtr->setCurrentRow(row);
                return;
            }
        }
    };
    connect(ctaEntry, &QPushButton::clicked, this, [goToPage]() { goToPage(QStringLiteral("Daily Entry")); });
    connect(ctaReports, &QPushButton::clicked, this, [goToPage]() { goToPage(QStringLiteral("Reports")); });
    connect(ctaInv, &QPushButton::clicked, this, [goToPage]() { goToPage(QStringLiteral("Inventory")); });
    connect(ctaSettings, &QPushButton::clicked, this, [goToPage]() { goToPage(QStringLiteral("Settings")); });

    animatePanel(*hero, 0);
    animatePanel(*onboard, 180);
    animatePanel(*kicker, 80);
    animatePanel(*headline, 160);
    animatePanel(*subline, 240);
    animatePanel(*chipsRow, 320);
    animatePanel(*ctaRow, 400);

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
    amount->setPrefix(QString(QChar(0x20B9)));
    QPushButton* save = new QPushButton(QStringLiteral("Save Entry"), entryPanel);
    save->setIcon(appIcon(QStringLiteral("save")));
    QPushButton* deleteButton = new QPushButton(QStringLiteral("Delete"), entryPanel);
    deleteButton->setObjectName(QStringLiteral("dangerButton"));
    deleteButton->setIcon(appIcon(QStringLiteral("delete")));
    deleteButton->setEnabled(false);
    deleteButton->setVisible(false);
    QPushButton* clear = new QPushButton(QStringLiteral("Clear"), entryPanel);
    clear->setObjectName(QStringLiteral("secondaryButton"));
    clear->setIcon(appIcon(QStringLiteral("clear")));
    QLabel* formStatus = new QLabel(QStringLiteral("Ready for a new entry"), entryPanel);
    formStatus->setObjectName(QStringLiteral("formStatus"));

    form->addWidget(new QLabel(QStringLiteral("Date"), entryPanel), 0, 0);
    form->addWidget(date, 1, 0);
    form->addWidget(new QLabel(QStringLiteral("Bill No."), entryPanel), 0, 1);
    form->addWidget(bill, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Party"), entryPanel), 0, 2);
    form->addWidget(party, 1, 2, 1, 4);
    form->addWidget(partyHint, 2, 2, 1, 4);
    form->addWidget(new QLabel(QStringLiteral("Type"), entryPanel), 3, 0);
    form->addWidget(type, 4, 0);
    form->addWidget(new QLabel(QStringLiteral("Mode"), entryPanel), 3, 1);
    form->addWidget(mode, 4, 1);
    form->addWidget(new QLabel(QStringLiteral("Amount"), entryPanel), 3, 2);
    form->addWidget(amount, 4, 2);
    form->addWidget(save, 4, 3);
    form->addWidget(clear, 4, 4);
    form->addWidget(deleteButton, 4, 5);
    form->addWidget(formStatus, 5, 0, 1, 6);
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
    refresh->setIcon(appIcon(QStringLiteral("refresh")));
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), tablePanel);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), tablePanel);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), tablePanel);
    print->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* importButton = new QPushButton(QStringLiteral("Import"), tablePanel);
    importButton->setObjectName(QStringLiteral("secondaryButton"));
    tableHeader->addWidget(transactionSearch);
    tableHeader->addWidget(refresh);
    tableHeader->addWidget(importButton);
    tableHeader->addWidget(exportExcel);
    tableHeader->addWidget(exportPdf);
    tableHeader->addWidget(print);
    tableLayout->addLayout(tableHeader);
    QTableWidget* table = createDataTable(tablePanel);
    loadTransactions(*table);
    tableLayout->addWidget(table);
    connect(transactionSearch, &QLineEdit::textChanged, this, [this, table](const QString& query) { applyTableSearch(*table, query); });
    connect(refresh, &QPushButton::clicked, this, [this, table, transactionSearch]() {
        loadTransactions(*table);
        applyTableSearch(*table, transactionSearch->text());
    });
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Daily Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Daily Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Daily Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    connect(importButton, &QPushButton::clicked, this, [this, table]() {
        try {
            importTransactionsWithValidation();
            loadTransactions(*table);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Import Transactions"), err);
        }
    });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+S")), [save]() { save->click(); });
    createShortcut(*page, QKeySequence(QStringLiteral("Alt+N")), [date]() { focusEntryWidget(*date); });
    createShortcut(*page, QKeySequence(QStringLiteral("Ctrl+K")), [transactionSearch]() { focusEntryWidget(*transactionSearch); });
    const std::shared_ptr<int> editingId = std::make_shared<int>(0);
    connect(clear, &QPushButton::clicked, this, [date, bill, party, type, mode, amount, save, deleteButton, formStatus, editingId]() {
        *editingId = 0;
        date->setDate(QDate::currentDate());
        bill->clear();
        party->setText(QStringLiteral("Customer"));
        type->setCurrentIndex(0);
        mode->setCurrentIndex(0);
        amount->setValue(0.0);
        save->setText(QStringLiteral("Save Entry"));
        deleteButton->setEnabled(false);
        deleteButton->setVisible(false);
        formStatus->setText(QStringLiteral("Ready for a new entry"));
        focusEntryWidget(*bill);
    });
    connect(table, &QTableWidget::itemSelectionChanged, this, [this, table, date, bill, party, type, mode, amount, save, deleteButton, formStatus, editingId]() {
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
            deleteButton->setEnabled(true);
            deleteButton->setVisible(true);
            formStatus->setText(QStringLiteral("Editing selected transaction"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Load Entry"), err);
        }
    });
    connect(save, &QPushButton::clicked, this, [this, date, bill, party, type, mode, amount, table, editingId, save, deleteButton, formStatus, transactionSearch]() {
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
                context_.services().transactions->editTransaction(domain::TransactionEditRequest{
                    *editingId,
                    domain::TransactionCreateRequest{
                        date->date(),
                        bill->text().trimmed(),
                        partyName,
                        transactionTypeFromText(type->currentText()),
                        paymentModeFromText(mode->currentText()),
                        amount->value()
                    },
                    adminUser
                });
                statusBar()->showMessage(QStringLiteral("Transaction updated"), 5000);
                formStatus->setText(QStringLiteral("Transaction updated"));
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
                formStatus->setText(QStringLiteral("Transaction saved"));
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
            deleteButton->setEnabled(false);
            deleteButton->setVisible(false);
        } catch (const std::exception& err) {
            formStatus->setText(QString::fromUtf8(err.what()));
            showError(QStringLiteral("Save Entry"), err);
        }
    });
    connect(deleteButton, &QPushButton::clicked, this, [this, table, editingId, save, deleteButton, formStatus, transactionSearch]() {
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
            deleteButton->setEnabled(false);
            deleteButton->setVisible(false);
            loadTransactions(*table);
            applyTableSearch(*table, transactionSearch->text());
            statusBar()->showMessage(QStringLiteral("Transaction deleted"), 5000);
            formStatus->setText(QStringLiteral("Transaction deleted"));
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
    layout->setContentsMargins(12, 6, 12, 8);
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
    QHBoxLayout* panelHeader = new QHBoxLayout();
    panelHeader->addWidget(createSectionTitle(QStringLiteral("Recent Transactions"), panel));
    panelHeader->addStretch(1);
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), panel);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), panel);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), panel);
    print->setObjectName(QStringLiteral("secondaryButton"));
    panelHeader->addWidget(exportExcel);
    panelHeader->addWidget(exportPdf);
    panelHeader->addWidget(print);
    panelLayout->addLayout(panelHeader);
    QTableWidget* table = createDataTable(panel);
    loadTransactions(*table);
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Dashboard Recent Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Dashboard Recent Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Dashboard Recent Transactions"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    panelLayout->addWidget(table);
    layout->addWidget(panel, 1);
    return page;
}

QWidget* DesktopApplication::buildPartiesPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 6, 12, 8);
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

    QHBoxLayout* tableActions = new QHBoxLayout();
    tableActions->addWidget(createSectionTitle(QStringLiteral("Parties"), page));
    tableActions->addStretch(1);
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), page);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), page);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), page);
    print->setObjectName(QStringLiteral("secondaryButton"));
    tableActions->addWidget(exportExcel);
    tableActions->addWidget(exportPdf);
    tableActions->addWidget(print);
    layout->addLayout(tableActions);

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
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Parties"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Parties"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Parties"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildReportPage(const QString& title, const QString& description) {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(4);

    layout->addWidget(createPageHeader(title, description, page));

    // Compact inline filter row (no panel wrapper to save vertical space)
    QWidget* filters = new QWidget(page);
    QHBoxLayout* filterLayout = new QHBoxLayout(filters);
    filterLayout->setContentsMargins(0, 4, 0, 4);
    filterLayout->setSpacing(8);
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
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), filters);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), filters);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), filters);
    print->setObjectName(QStringLiteral("secondaryButton"));
    filterLayout->addWidget(search, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("Start"), filters));
    filterLayout->addWidget(start);
    filterLayout->addWidget(new QLabel(QStringLiteral("End"), filters));
    filterLayout->addWidget(end);
    filterLayout->addWidget(refresh);
    filterLayout->addWidget(exportExcel);
    filterLayout->addWidget(exportPdf);
    filterLayout->addWidget(print);
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
        QPushButton* resetCash = new QPushButton(QStringLiteral("Reset Cash"), cashPanel);
        resetCash->setObjectName(QStringLiteral("secondaryButton"));
        cashLayout->addWidget(new QLabel(QStringLiteral("Cash Date"), cashPanel));
        cashLayout->addWidget(cashDate);
        cashLayout->addWidget(new QLabel(QStringLiteral("Cash In Hand"), cashPanel));
        cashLayout->addWidget(cashAmount);
        cashLayout->addStretch(1);
        cashLayout->addWidget(saveCash);
        cashLayout->addWidget(resetCash);
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
        connect(resetCash, &QPushButton::clicked, this, [this, cashDate, refresh]() {
            try {
                context_.services().reports->resetCashInHand(cashDate->date());
                refresh->click();
                statusBar()->showMessage(QStringLiteral("Cash in hand reset"), 7000);
            } catch (const std::exception& err) {
                showError(QStringLiteral("Reset Cash In Hand"), err);
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
    connect(exportPdf, &QPushButton::clicked, this, [this, table, title]() {
        try {
            exportTableToPdf(*table, title);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table, title]() {
        try {
            exportTableToExcel(*table, title);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table, title]() {
        try {
            printTable(*table, title);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildAuditPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 6, 12, 8);
    layout->setSpacing(6);
    layout->addWidget(createPageHeader(QStringLiteral("Audit Logs"), QStringLiteral("Administrative activity and native service events."), page));
    layout->addWidget(createContextBar(context_, page));

    QHBoxLayout* actions = new QHBoxLayout();
    actions->addStretch(1);
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), page);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), page);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), page);
    print->setObjectName(QStringLiteral("secondaryButton"));
    actions->addWidget(exportExcel);
    actions->addWidget(exportPdf);
    actions->addWidget(print);
    layout->addLayout(actions);

    QTableWidget* table = createDataTable(page);
    loadAuditLogs(*table);
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Audit Logs"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Audit Logs"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Audit Logs"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    layout->addWidget(table, 1);
    return page;
}

QWidget* DesktopApplication::buildSettingsPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 6, 12, 8);
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
    domain::DatabaseConfig config{
        QStringLiteral("localhost"),
        QStringLiteral("Finlogs"),
        QString(),
        QString(),
        QStringLiteral("{ODBC Driver 17 for SQL Server}"),
        QStringLiteral("http://127.0.0.1:8000"),
        QStringLiteral("D:/finlogs"),
        true
    };
    try {
        config = context_.services().config->readDatabaseConfig();
    } catch (const std::exception& err) {
        showError(QStringLiteral("Read Database Config"), err);
    }
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
    QPushButton* autoBackup = new QPushButton(QStringLiteral("Auto Backup"), panel);
    autoBackup->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* openBackupDir = new QPushButton(QStringLiteral("Open Backup Folder"), panel);
    openBackupDir->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* restore = new QPushButton(QStringLiteral("Restore Backup"), panel);
    restore->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* checkUpdates = new QPushButton(QStringLiteral("Check Updates"), panel);
    checkUpdates->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* installUpdate = new QPushButton(QStringLiteral("Install Update"), panel);
    installUpdate->setObjectName(QStringLiteral("secondaryButton"));
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
    form->addWidget(autoBackup, 6, 1);
    form->addWidget(openBackupDir, 6, 2);
    form->addWidget(restore, 6, 3);
    form->addWidget(checkUpdates, 7, 0);
    form->addWidget(installUpdate, 7, 1);

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
    connect(autoBackup, &QPushButton::clicked, this, [this]() {
        try {
            const domain::BackupResult result = context_.services().backups->autoBackup();
            QMessageBox::information(this, QStringLiteral("Auto Backup"), result.status + QStringLiteral("\n") + result.path);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Auto Backup"), err);
        }
    });
    connect(openBackupDir, &QPushButton::clicked, this, [this, backupDir]() {
        const QString path = backupDir->text().trimmed();
        if (path.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Backup directory is empty"), 5000);
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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
    connect(checkUpdates, &QPushButton::clicked, this, [this]() {
        try {
            context_.services().updates->checkForUpdates();
            statusBar()->showMessage(QStringLiteral("Update check completed"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Check Updates"), err);
        }
    });
    connect(installUpdate, &QPushButton::clicked, this, [this]() {
        try {
            context_.services().updates->restartAndInstall();
        } catch (const std::exception& err) {
            showError(QStringLiteral("Install Update"), err);
        }
    });
    layout->addWidget(panel);

    QFrame* cashSettingsPanel = createPanel(page);
    QHBoxLayout* cashSettingsLayout = new QHBoxLayout(cashSettingsPanel);
    cashSettingsLayout->setContentsMargins(14, 12, 14, 12);
    cashSettingsLayout->setSpacing(10);
    QLabel* cashSettingsTitle = new QLabel(QStringLiteral("Opening Cash"), cashSettingsPanel);
    cashSettingsTitle->setObjectName(QStringLiteral("sectionTitle"));
    QDoubleSpinBox* openingCash = new QDoubleSpinBox(cashSettingsPanel);
    openingCash->setMaximum(999999999.0);
    openingCash->setDecimals(2);
    QPushButton* saveOpeningCash = new QPushButton(QStringLiteral("Save Opening Cash"), cashSettingsPanel);
    saveOpeningCash->setObjectName(QStringLiteral("secondaryButton"));
    try {
        openingCash->setValue(context_.services().reports->openingCashSeed());
    } catch (const std::exception& err) {
        showError(QStringLiteral("Opening Cash"), err);
    }
    cashSettingsLayout->addWidget(cashSettingsTitle);
    cashSettingsLayout->addWidget(openingCash);
    cashSettingsLayout->addStretch(1);
    cashSettingsLayout->addWidget(saveOpeningCash);
    connect(saveOpeningCash, &QPushButton::clicked, this, [this, openingCash]() {
        try {
            context_.services().reports->saveOpeningCashSeed(openingCash->value());
            statusBar()->showMessage(QStringLiteral("Opening cash saved"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Save Opening Cash"), err);
        }
    });
    layout->addWidget(cashSettingsPanel);

    QFrame* userPanel = createPanel(page);
    QVBoxLayout* userLayout = new QVBoxLayout(userPanel);
    userLayout->setContentsMargins(14, 12, 14, 12);
    userLayout->setSpacing(10);
    QLabel* userTitle = new QLabel(QStringLiteral("User Management"), userPanel);
    userTitle->setObjectName(QStringLiteral("sectionTitle"));
    userLayout->addWidget(userTitle);

    QGridLayout* userForm = new QGridLayout();
    QLineEdit* newUsername = new QLineEdit(userPanel);
    newUsername->setPlaceholderText(QStringLiteral("Username"));
    QLineEdit* newPassword = new QLineEdit(userPanel);
    newPassword->setPlaceholderText(QStringLiteral("Password"));
    newPassword->setEchoMode(QLineEdit::Password);
    QComboBox* newRole = new QComboBox(userPanel);
    newRole->addItems({QStringLiteral("accounts"), QStringLiteral("admin")});
    QPushButton* createUser = new QPushButton(QStringLiteral("Create User"), userPanel);
    QPushButton* changePassword = new QPushButton(QStringLiteral("Change Password"), userPanel);
    changePassword->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* deleteUser = new QPushButton(QStringLiteral("Delete Selected"), userPanel);
    deleteUser->setObjectName(QStringLiteral("dangerButton"));
    QPushButton* refreshUsers = new QPushButton(QStringLiteral("Refresh"), userPanel);
    refreshUsers->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportUsers = new QPushButton(QStringLiteral("Export PDF"), userPanel);
    exportUsers->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportUsersExcel = new QPushButton(QStringLiteral("Export Excel"), userPanel);
    exportUsersExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* printUsers = new QPushButton(QStringLiteral("Print"), userPanel);
    printUsers->setObjectName(QStringLiteral("secondaryButton"));
    userForm->addWidget(new QLabel(QStringLiteral("Username"), userPanel), 0, 0);
    userForm->addWidget(new QLabel(QStringLiteral("Password"), userPanel), 0, 1);
    userForm->addWidget(new QLabel(QStringLiteral("Role"), userPanel), 0, 2);
    userForm->addWidget(newUsername, 1, 0);
    userForm->addWidget(newPassword, 1, 1);
    userForm->addWidget(newRole, 1, 2);
    userForm->addWidget(createUser, 1, 3);
    userForm->addWidget(changePassword, 1, 4);
    userForm->addWidget(deleteUser, 1, 5);
    userForm->addWidget(refreshUsers, 1, 6);
    userForm->addWidget(exportUsersExcel, 1, 7);
    userForm->addWidget(exportUsers, 1, 8);
    userForm->addWidget(printUsers, 1, 9);
    userLayout->addLayout(userForm);

    QTableWidget* usersTable = createDataTable(userPanel);
    usersTable->setMaximumHeight(180);
    const QStringList userHeaders = {QStringLiteral("username"), QStringLiteral("role")};
    auto loadUsers = [this, usersTable, userHeaders]() {
        ensureTableHeaders(*usersTable, userHeaders);
        try {
            setTableRows(*usersTable, userHeaders, context_.services().users->listUsers(QStringLiteral("native-admin")));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Users"), err);
        }
    };
    loadUsers();
    userLayout->addWidget(usersTable);
    connect(refreshUsers, &QPushButton::clicked, this, loadUsers);
    connect(createUser, &QPushButton::clicked, this, [this, newUsername, newPassword, newRole, loadUsers]() {
        try {
            const QString username = newUsername->text().trimmed();
            const QString password = newPassword->text();
            const domain::UserRole role = newRole->currentText() == QStringLiteral("admin")
                ? domain::UserRole::Admin
                : domain::UserRole::Accounts;
            context_.services().users->createUser(username, password, role, QStringLiteral("native-admin"));
            newUsername->clear();
            newPassword->clear();
            loadUsers();
            statusBar()->showMessage(QStringLiteral("User created"), 6000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Create User"), err);
        }
    });
    connect(changePassword, &QPushButton::clicked, this, [this, newUsername, newPassword]() {
        try {
            context_.services().users->changePassword(newUsername->text().trimmed(), newPassword->text(), QStringLiteral("native-admin"));
            newPassword->clear();
            statusBar()->showMessage(QStringLiteral("Password changed"), 6000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Change Password"), err);
        }
    });
    connect(deleteUser, &QPushButton::clicked, this, [this, usersTable, loadUsers]() {
        const QList<QTableWidgetItem*> selected = usersTable->selectedItems();
        if (selected.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Select a user first"), 5000);
            return;
        }
        const QTableWidgetItem* usernameItem = usersTable->item(selected.first()->row(), 0);
        const QString username = usernameItem ? usernameItem->text().trimmed() : QString();
        if (username.isEmpty() || username == QStringLiteral("admin")) {
            statusBar()->showMessage(QStringLiteral("Default admin user cannot be deleted"), 5000);
            return;
        }
        const QMessageBox::StandardButton result = QMessageBox::question(
            this,
            QStringLiteral("Delete User"),
            QStringLiteral("Delete user %1?").arg(username),
            QMessageBox::Yes | QMessageBox::No
        );
        if (result != QMessageBox::Yes) {
            return;
        }
        try {
            context_.services().users->deleteUser(username, QStringLiteral("native-admin"));
            loadUsers();
            statusBar()->showMessage(QStringLiteral("User deleted"), 6000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Delete User"), err);
        }
    });
    connect(exportUsers, &QPushButton::clicked, this, [this, usersTable]() {
        try {
            exportTableToPdf(*usersTable, QStringLiteral("Users"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportUsersExcel, &QPushButton::clicked, this, [this, usersTable]() {
        try {
            exportTableToExcel(*usersTable, QStringLiteral("Users"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(printUsers, &QPushButton::clicked, this, [this, usersTable]() {
        try {
            printTable(*usersTable, QStringLiteral("Users"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    layout->addWidget(userPanel);

    QFrame* importPanel = createPanel(page);
    QHBoxLayout* importLayout = new QHBoxLayout(importPanel);
    importLayout->setContentsMargins(14, 12, 14, 12);
    importLayout->setSpacing(10);
    QLabel* importTitle = new QLabel(QStringLiteral("Data Import"), importPanel);
    importTitle->setObjectName(QStringLiteral("sectionTitle"));
    QPushButton* importTransactions = new QPushButton(QStringLiteral("Import Transactions"), importPanel);
    importTransactions->setObjectName(QStringLiteral("secondaryButton"));
    importLayout->addWidget(importTitle);
    importLayout->addStretch(1);
    importLayout->addWidget(importTransactions);
    connect(importTransactions, &QPushButton::clicked, this, [this]() {
        try {
            importTransactionsWithValidation();
        } catch (const std::exception& err) {
            showError(QStringLiteral("Import Transactions"), err);
        }
    });
    layout->addWidget(importPanel);
    layout->addStretch(1);
    return page;
}

QWidget* DesktopApplication::buildInventoryPage() {
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 6, 12, 8);
    layout->setSpacing(6);
    layout->addWidget(createPageHeader(QStringLiteral("Inventory Management"), QStringLiteral("Monthly stock quantities and purchases per day. Tab between Qty and Purchase fields."), page));
    layout->addWidget(createContextBar(context_, page));

    const QString financialYear = currentFinancialYearValue();
    QFrame* toolbar = createPanel(page);
    QVBoxLayout* toolbarOuter = new QVBoxLayout(toolbar);
    toolbarOuter->setContentsMargins(14, 12, 14, 12);
    toolbarOuter->setSpacing(10);
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);
    QComboBox* month = new QComboBox(toolbar);
    month->addItems(monthNames());
    month->setCurrentIndex(QDate::currentDate().month() - 1);
    QLineEdit* product = new QLineEdit(toolbar);
    product->setPlaceholderText(QStringLiteral("Product name"));
    QPushButton* add = new QPushButton(QStringLiteral("Add Product"), toolbar);
    add->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* addGap = new QPushButton(QStringLiteral("Add Gap"), toolbar);
    addGap->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* cleanEmpty = new QPushButton(QStringLiteral("Clean Empty Rows"), toolbar);
    cleanEmpty->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), toolbar);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* preview = new QPushButton(QStringLiteral("Preview PDF"), toolbar);
    preview->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), toolbar);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), toolbar);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), toolbar);
    print->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* mail = new QPushButton(QStringLiteral("Send PDF"), toolbar);
    mail->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* save = new QPushButton(QStringLiteral("Save Inventory"), toolbar);
    QCheckBox* onlyReorder = new QCheckBox(QStringLiteral("Reorder only"), toolbar);
    QCheckBox* includeValue = new QCheckBox(QStringLiteral("Include stock value"), toolbar);
    includeValue->setChecked(true);
    QComboBox* viewMode = new QComboBox(toolbar);
    viewMode->addItems({QStringLiteral("Stock Quantities"), QStringLiteral("Purchase Entries"), QStringLiteral("Both")});
    viewMode->setCurrentIndex(2);
    QPushButton* addPurchase = new QPushButton(QStringLiteral("Record Purchase"), toolbar);
    addPurchase->setObjectName(QStringLiteral("secondaryButton"));
    QLabel* periodChip = new QLabel(inventoryDateRangeText(financialYear, month->currentIndex() + 1), toolbar);
    QLabel* tableSummary = new QLabel(QStringLiteral("No inventory loaded"), toolbar);
    periodChip->setObjectName(QStringLiteral("contextChip"));
    tableSummary->setObjectName(QStringLiteral("pageMeta"));
    toolbarLayout->addWidget(new QLabel(QStringLiteral("Month"), toolbar));
    toolbarLayout->addWidget(month);
    toolbarLayout->addWidget(product, 1);
    toolbarLayout->addWidget(add);
    toolbarLayout->addWidget(addPurchase);
    toolbarLayout->addWidget(addGap);
    toolbarLayout->addWidget(cleanEmpty);
    toolbarLayout->addWidget(refresh);
    toolbarLayout->addWidget(preview);
    toolbarLayout->addWidget(exportExcel);
    toolbarLayout->addWidget(exportPdf);
    toolbarLayout->addWidget(print);
    toolbarLayout->addWidget(mail);
    toolbarLayout->addWidget(save);
    QHBoxLayout* toolbarMeta = new QHBoxLayout();
    toolbarMeta->setSpacing(10);
    toolbarMeta->addWidget(periodChip);
    toolbarMeta->addWidget(new QLabel(QStringLiteral("View"), toolbar));
    toolbarMeta->addWidget(viewMode);
    toolbarMeta->addWidget(onlyReorder);
    toolbarMeta->addWidget(includeValue);
    toolbarMeta->addWidget(tableSummary, 1);
    toolbarOuter->addLayout(toolbarLayout);
    toolbarOuter->addLayout(toolbarMeta);
    layout->addWidget(toolbar);

    // Inventory metrics panel (mirrors the PDF report header cards)
    QFrame* metricsRow = createAccentPanel(page);
    QHBoxLayout* metricsLayout = new QHBoxLayout(metricsRow);
    metricsLayout->setContentsMargins(16, 12, 16, 12);
    metricsLayout->setSpacing(20);
    QLabel* totalQtyValue = new QLabel(QStringLiteral("0.00"), metricsRow);
    QLabel* totalQtyLabel = new QLabel(QStringLiteral("Current Quantity"), metricsRow);
    QLabel* purchaseInValue = new QLabel(QStringLiteral("0.00"), metricsRow);
    QLabel* purchaseInLabel = new QLabel(QStringLiteral("Purchase In (Today)"), metricsRow);
    QLabel* avgDailyValue = new QLabel(QStringLiteral("0.0"), metricsRow);
    QLabel* avgDailyLabel = new QLabel(QStringLiteral("Avg Daily Movement"), metricsRow);
    QLabel* reorderValue = new QLabel(QStringLiteral("0"), metricsRow);
    QLabel* reorderLabel = new QLabel(QStringLiteral("Reorder Products"), metricsRow);
    totalQtyValue->setObjectName(QStringLiteral("metricValue"));
    totalQtyLabel->setObjectName(QStringLiteral("metricLabel"));
    purchaseInValue->setObjectName(QStringLiteral("metricValue"));
    purchaseInLabel->setObjectName(QStringLiteral("metricLabel"));
    avgDailyValue->setObjectName(QStringLiteral("metricValue"));
    avgDailyLabel->setObjectName(QStringLiteral("metricLabel"));
    reorderValue->setObjectName(QStringLiteral("metricValue"));
    reorderLabel->setObjectName(QStringLiteral("metricLabel"));
    auto addMetricTileToLayout = [&metricsLayout](QLabel* value, QLabel* label) {
        QVBoxLayout* tile = new QVBoxLayout();
        tile->setSpacing(2);
        tile->addWidget(value);
        tile->addWidget(label);
        metricsLayout->addLayout(tile);
    };
    addMetricTileToLayout(totalQtyValue, totalQtyLabel);
    addMetricTileToLayout(purchaseInValue, purchaseInLabel);
    addMetricTileToLayout(avgDailyValue, avgDailyLabel);
    addMetricTileToLayout(reorderValue, reorderLabel);
    metricsLayout->addStretch(1);
    layout->addWidget(metricsRow);

    // Inventory table with frozen Product/Cost/Min Stock pane
    // Uses two side-by-side tables: left (frozen) + right (scrollable day columns)
    QSplitter* tableSplitter = new QSplitter(Qt::Horizontal, page);
    tableSplitter->setChildrenCollapsible(false);
    tableSplitter->setHandleWidth(2);
    tableSplitter->setStyleSheet(QStringLiteral("QSplitter::handle { background: #6366f1; }"));

    QTableWidget* frozenTable = createInventoryTable(tableSplitter);
    frozenTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    frozenTable->setObjectName(QStringLiteral("inventoryFrozen"));

    QTableWidget* table = createInventoryTable(tableSplitter);

    tableSplitter->addWidget(frozenTable);
    tableSplitter->addWidget(table);
    tableSplitter->setSizes({330, 700});

    // Load data into main table, then mirror frozen columns
    loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);

    auto syncFrozenTable = [table, frozenTable]() {
        frozenTable->setRowCount(table->rowCount());
        frozenTable->setColumnCount(4);
        QStringList fHeaders;
        for (int c = 0; c < 4; ++c) {
            QTableWidgetItem* h = table->horizontalHeaderItem(c);
            fHeaders.append(h ? h->text() : QString());
        }
        frozenTable->setHorizontalHeaderLabels(fHeaders);
        frozenTable->setColumnHidden(0, true);
        frozenTable->setColumnWidth(1, 180);
        frozenTable->setColumnWidth(2, 70);
        frozenTable->setColumnWidth(3, 70);
        for (int r = 0; r < table->rowCount(); ++r) {
            for (int c = 0; c < 4; ++c) {
                QTableWidgetItem* src = table->item(r, c);
                QTableWidgetItem* dst = src ? new QTableWidgetItem(*src) : new QTableWidgetItem();
                if (c == 0) dst->setFlags(Qt::ItemIsEnabled);
                frozenTable->setItem(r, c, dst);
            }
        }
    };
    syncFrozenTable();

    // Sync vertical scroll between the two tables
    QObject::connect(table->verticalScrollBar(), &QScrollBar::valueChanged,
        frozenTable->verticalScrollBar(), &QScrollBar::setValue);
    QObject::connect(frozenTable->verticalScrollBar(), &QScrollBar::valueChanged,
        table->verticalScrollBar(), &QScrollBar::setValue);

    layout->addWidget(tableSplitter, 1);

    auto refreshSummary = [table, tableSummary, periodChip, financialYear, month,
                           totalQtyValue, purchaseInValue, avgDailyValue, reorderValue]() {
        int productCount = 0;
        double grandTotal = 0.0;
        double grandPurchaseToday = 0.0;
        double totalOutflow = 0.0;
        int outflowDays = 0;
        int reorderCount = 0;
        const int today = QDate::currentDate().day();
        for (int rowIndex = 0; rowIndex < table->rowCount(); rowIndex += 1) {
            const QTableWidgetItem* nameItem = table->item(rowIndex, 1);
            if (!nameItem || nameItem->text().trimmed().isEmpty() || nameItem->text().startsWith(QStringLiteral("No inventory rows yet"))) {
                continue;
            }
            productCount += 1;
            // Current qty = today's qty column
            const int todayQtyCol = 4 + (today - 1) * 2;
            const double currentQty = (todayQtyCol < table->columnCount() && table->item(rowIndex, todayQtyCol))
                ? table->item(rowIndex, todayQtyCol)->text().toDouble() : 0.0;
            grandTotal += currentQty;
            // Today's purchase
            const int purchaseTodayCol = 4 + (today - 1) * 2 + 1;
            const double purchaseToday = (purchaseTodayCol < table->columnCount() && table->item(rowIndex, purchaseTodayCol))
                ? table->item(rowIndex, purchaseTodayCol)->text().toDouble() : 0.0;
            grandPurchaseToday += purchaseToday;
            // Avg daily outflow: sum of stock drops across days
            double prevQty = 0.0;
            for (int day = 1; day <= today; day += 1) {
                const int col = 4 + (day - 1) * 2;
                const double qty = (col < table->columnCount() && table->item(rowIndex, col))
                    ? table->item(rowIndex, col)->text().toDouble() : 0.0;
                if (day > 1 && prevQty > qty) {
                    totalOutflow += (prevQty - qty);
                }
                prevQty = qty;
            }
            // Reorder check: min_stock vs current qty
            const double minStock = table->item(rowIndex, 3) ? table->item(rowIndex, 3)->text().toDouble() : 0.0;
            if (minStock > 0.0 && currentQty < minStock) {
                reorderCount += 1;
            }
        }
        outflowDays = qMax(today - 1, 1);
        const double avgDaily = outflowDays > 0 ? totalOutflow / outflowDays : 0.0;
        tableSummary->setText(QStringLiteral("%1 products | %2 days | Edit quantities directly | Tab between Qty and Purchase")
            .arg(productCount).arg(daysInInventoryMonth(month->currentIndex() + 1)));
        periodChip->setText(inventoryDateRangeText(financialYear, month->currentIndex() + 1));
        totalQtyValue->setText(QStringLiteral("%1").arg(grandTotal, 0, 'f', 2));
        purchaseInValue->setText(QStringLiteral("%1").arg(grandPurchaseToday, 0, 'f', 2));
        avgDailyValue->setText(QStringLiteral("%1").arg(avgDaily, 0, 'f', 1));
        reorderValue->setText(QString::number(reorderCount));
    };
    refreshSummary();

    // View mode toggle: show/hide qty vs purchase columns
    connect(viewMode, &QComboBox::currentIndexChanged, this, [table, month](int index) {
        const int visibleDays = daysInInventoryMonth(month->currentIndex() + 1);
        for (int day = 1; day <= 31; ++day) {
            const bool dayVisible = (day <= visibleDays);
            const int qtyCol = 4 + (day - 1) * 2;
            const int purchCol = 4 + (day - 1) * 2 + 1;
            // index 0 = Stock only, 1 = Purchase only, 2 = Both
            table->setColumnHidden(qtyCol, !dayVisible || index == 1);
            table->setColumnHidden(purchCol, !dayVisible || index == 0);
        }
    });

    // Record Purchase dialog
    connect(addPurchase, &QPushButton::clicked, this, [this, table, month]() {
        const int selectedRow = table->currentRow();
        if (selectedRow < 0 || !table->item(selectedRow, 1) || table->item(selectedRow, 1)->text().trimmed().isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Select a product row first"), 5000);
            return;
        }
        const QString productName = table->item(selectedRow, 1)->text();
        const int visibleDays = daysInInventoryMonth(month->currentIndex() + 1);

        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Record Purchase - %1").arg(productName));
        dialog.setModal(true);
        dialog.resize(420, 220);
        QVBoxLayout* dlgLayout = new QVBoxLayout(&dialog);
        dlgLayout->setContentsMargins(20, 18, 20, 18);
        dlgLayout->setSpacing(12);
        QLabel* dlgTitle = new QLabel(QStringLiteral("Record Purchase"), &dialog);
        dlgTitle->setObjectName(QStringLiteral("sectionTitle"));
        QLabel* dlgProduct = new QLabel(productName, &dialog);
        dlgProduct->setObjectName(QStringLiteral("pageMeta"));
        dlgLayout->addWidget(dlgTitle);
        dlgLayout->addWidget(dlgProduct);

        QGridLayout* form = new QGridLayout();
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(8);
        QSpinBox* dayInput = new QSpinBox(&dialog);
        dayInput->setRange(1, visibleDays);
        dayInput->setValue(qMin(QDate::currentDate().day(), visibleDays));
        QDoubleSpinBox* qtyInput = new QDoubleSpinBox(&dialog);
        qtyInput->setMaximum(999999.0);
        qtyInput->setDecimals(2);
        qtyInput->setPrefix(QStringLiteral("+ "));
        form->addWidget(new QLabel(QStringLiteral("Day of Month"), &dialog), 0, 0);
        form->addWidget(dayInput, 0, 1);
        form->addWidget(new QLabel(QStringLiteral("Purchase Quantity"), &dialog), 1, 0);
        form->addWidget(qtyInput, 1, 1);
        dlgLayout->addLayout(form);

        QHBoxLayout* actions = new QHBoxLayout();
        actions->addStretch(1);
        QPushButton* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
        cancel->setObjectName(QStringLiteral("secondaryButton"));
        QPushButton* apply = new QPushButton(QStringLiteral("Apply"), &dialog);
        actions->addWidget(cancel);
        actions->addWidget(apply);
        dlgLayout->addLayout(actions);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(qtyInput, &QDoubleSpinBox::editingFinished, apply, [apply]() { apply->setFocus(); });
        connect(apply, &QPushButton::clicked, &dialog, [&]() {
            const int day = dayInput->value();
            const int purchaseCol = 4 + (day - 1) * 2 + 1;
            QTableWidgetItem* cell = table->item(selectedRow, purchaseCol);
            if (!cell) {
                cell = new QTableWidgetItem();
                cell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
                cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table->setItem(selectedRow, purchaseCol, cell);
            }
            const double existing = cell->text().toDouble();
            cell->setText(QString::number(existing + qtyInput->value(), 'f', 2));
            dialog.accept();
        });

        QTimer::singleShot(0, qtyInput, [qtyInput]() { qtyInput->setFocus(); qtyInput->selectAll(); });
        dialog.exec();
    });

    connect(month, &QComboBox::currentIndexChanged, this, [this, table, financialYear, month, syncFrozenTable](int) {
        loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
        syncFrozenTable();
    });
    connect(month, &QComboBox::currentIndexChanged, this, [refreshSummary](int) {
        refreshSummary();
    });
    connect(refresh, &QPushButton::clicked, this, [this, table, financialYear, month, refreshSummary, syncFrozenTable]() {
        loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
        syncFrozenTable();
        refreshSummary();
    });
    connect(add, &QPushButton::clicked, this, [table, product, refreshSummary, syncFrozenTable]() {
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
        syncFrozenTable();
    });
    connect(addGap, &QPushButton::clicked, this, [table, refreshSummary, syncFrozenTable]() {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, createTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        table->setItem(row, 1, createTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        table->setItem(row, 2, createNumberTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        table->setItem(row, 3, createNumberTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        for (int column = 4; column < table->columnCount(); column += 1) {
            table->setItem(row, column, createNumberTableItem(QString(), Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable));
        }
        table->setCurrentCell(row, 1);
        table->editItem(table->item(row, 1));
        refreshSummary();
        syncFrozenTable();
    });
    connect(cleanEmpty, &QPushButton::clicked, this, [table, refreshSummary, syncFrozenTable]() {
        for (int row = table->rowCount() - 1; row >= 0; row -= 1) {
            const QTableWidgetItem* item = table->item(row, 1);
            if (!item || item->text().trimmed().isEmpty()) {
                table->removeRow(row);
            }
        }
        refreshSummary();
        syncFrozenTable();
    });
    connect(product, &QLineEdit::returnPressed, add, &QPushButton::click);
    connect(save, &QPushButton::clicked, this, [this, table, financialYear, month, refreshSummary, syncFrozenTable]() {
        try {
            context_.services().inventory->saveSnapshot(financialYear, month->currentIndex() + 1, inventoryRowsFromTable(*table));
            loadInventorySnapshot(*table, financialYear, month->currentIndex() + 1);
            syncFrozenTable();
            refreshSummary();
            statusBar()->showMessage(QStringLiteral("Inventory saved"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Save Inventory"), err);
        }
    });
    connect(preview, &QPushButton::clicked, this, [this, table, financialYear, month, onlyReorder, includeValue]() {
        try {
            const QJsonArray sourceRows = inventoryRowsFromTable(*table);
            const QVector<domain::InventoryProductRow> rows = inventoryProductRowsFromJson(sourceRows);
            const QByteArray pdf = context_.services().inventory->buildPdfPreview(domain::InventoryPdfMailRequest{
                QString(), QString(), QString(), QString(), QStringLiteral("smtp.gmail.com"), 587,
                QStringLiteral("Inventory Report"), QString(), financialYear, month->currentText(), QStringLiteral("monthly"), onlyReorder->isChecked(), includeValue->isChecked(), rows
            });
            // Write to a temp file and open it directly so the user always sees the freshly
            // generated preview (avoids confusion with stale saved files).
            const QString tempPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                .filePath(QStringLiteral("MFinlogs_Inventory_Preview_%1.pdf")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
            QFile tempFile(tempPath);
            if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                throw std::runtime_error(QStringLiteral("Could not write preview PDF: %1").arg(tempPath).toStdString());
            }
            tempFile.write(pdf);
            tempFile.close();
            QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
            statusBar()->showMessage(QStringLiteral("Inventory PDF preview opened"), 7000);
        } catch (const std::exception& err) {
            showError(QStringLiteral("Inventory PDF"), err);
        }
    });
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Inventory Management"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Inventory Management"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Inventory Management"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
    });
    connect(mail, &QPushButton::clicked, this, [this, table, financialYear, month, onlyReorder, includeValue]() {
        std::optional<InventoryMailProfile> storedProfile;
        try {
            storedProfile = readInventoryMailProfile();
        } catch (const std::exception& err) {
            showError(QStringLiteral("Inventory Mail Profile"), err);
            return;
        }

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
        QComboBox* averageMode = new QComboBox(&dialog);
        averageMode->addItems({QStringLiteral("monthly"), QStringLiteral("last7")});
        QCheckBox* rememberProfile = new QCheckBox(QStringLiteral("Remember address and SMTP settings"), &dialog);
        password->setEchoMode(QLineEdit::Password);
        if (storedProfile.has_value()) {
            to->setText(storedProfile->toEmail);
            cc->setText(storedProfile->ccEmail);
            from->setText(storedProfile->senderEmail);
            host->setText(storedProfile->smtpHost);
            port->setText(storedProfile->smtpPort);
            subject->setText(storedProfile->subject);
            const int averageIndex = averageMode->findText(storedProfile->averageMode);
            if (averageIndex >= 0) {
                averageMode->setCurrentIndex(averageIndex);
            }
            rememberProfile->setChecked(true);
        }
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
        form->addWidget(new QLabel(QStringLiteral("Average Mode"), &dialog), 8, 0);
        form->addWidget(averageMode, 8, 1);
        form->addWidget(rememberProfile, 9, 1);
        form->addWidget(cancel, 10, 0);
        form->addWidget(send, 10, 1);
        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(send, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        try {
            const QJsonArray sourceRows = inventoryRowsFromTable(*table);
            const QVector<domain::InventoryProductRow> rows = inventoryProductRowsFromJson(sourceRows);
            context_.services().inventory->sendPdfMail(domain::InventoryPdfMailRequest{
                to->text(), cc->text(), from->text(), password->text(), host->text(), port->text().toInt(),
                subject->text(), notes->text(), financialYear, month->currentText(), averageMode->currentText(), onlyReorder->isChecked(), includeValue->isChecked(), rows
            });
            if (rememberProfile->isChecked()) {
                writeInventoryMailProfile(InventoryMailProfile{
                    to->text().trimmed(),
                    cc->text().trimmed(),
                    from->text().trimmed(),
                    host->text().trimmed(),
                    port->text().trimmed(),
                    subject->text().trimmed(),
                    averageMode->currentText()
                });
                statusBar()->showMessage(QStringLiteral("Inventory PDF sent. Mail profile saved without password."), 9000);
            } else {
                statusBar()->showMessage(QStringLiteral("Inventory PDF sent"), 9000);
            }
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
    layout->setContentsMargins(12, 6, 12, 8);
    layout->setSpacing(6);
    layout->addWidget(createPageHeader(QStringLiteral("Stock Value Report"), QStringLiteral("Daily stock quantity and value from saved inventory snapshots."), page));
    layout->addWidget(createContextBar(context_, page));

    const QString financialYear = currentFinancialYearValue();
    QFrame* toolbar = createPanel(page);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    QComboBox* month = new QComboBox(toolbar);
    month->addItems(monthNames());
    month->setCurrentIndex(QDate::currentDate().month() - 1);
    QPushButton* refresh = new QPushButton(QStringLiteral("Refresh"), toolbar);
    refresh->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportPdf = new QPushButton(QStringLiteral("Export PDF"), toolbar);
    exportPdf->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* exportExcel = new QPushButton(QStringLiteral("Export Excel"), toolbar);
    exportExcel->setObjectName(QStringLiteral("secondaryButton"));
    QPushButton* print = new QPushButton(QStringLiteral("Print"), toolbar);
    print->setObjectName(QStringLiteral("secondaryButton"));
    toolbarLayout->addWidget(new QLabel(QStringLiteral("Month"), toolbar));
    toolbarLayout->addWidget(month);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(refresh);
    toolbarLayout->addWidget(exportExcel);
    toolbarLayout->addWidget(exportPdf);
    toolbarLayout->addWidget(print);
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
    connect(exportPdf, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToPdf(*table, QStringLiteral("Stock Value Report"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export PDF"), err);
        }
    });
    connect(exportExcel, &QPushButton::clicked, this, [this, table]() {
        try {
            exportTableToExcel(*table, QStringLiteral("Stock Value Report"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Export Excel"), err);
        }
    });
    connect(print, &QPushButton::clicked, this, [this, table]() {
        try {
            printTable(*table, QStringLiteral("Stock Value Report"));
        } catch (const std::exception& err) {
            showError(QStringLiteral("Print"), err);
        }
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
        table.horizontalHeader()->setStretchLastSection(false);
        for (int columnIndex = 0; columnIndex < table.columnCount(); columnIndex += 1) {
            table.horizontalHeader()->setSectionResizeMode(columnIndex, QHeaderView::Interactive);
        }
        table.horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table.setColumnWidth(0, 120);
        table.setColumnWidth(1, 140);
        table.setColumnWidth(3, 120);
        table.setColumnWidth(4, 120);
        table.setColumnWidth(5, 140);
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
    // Interleaved column layout: Product, Cost, Min Stock, then for each day: Qty, Purchase
    // Col 0: row_id (hidden)
    // Col 1: Product, Col 2: Cost, Col 3: Min Stock
    // Col 4+(d-1)*2: Qty for day d, Col 4+(d-1)*2+1: Purchase for day d
    const int visibleDays = daysInInventoryMonth(month);
    const QString monthPad = QString::number(month).rightJustified(2, QLatin1Char('0'));
    const int totalCols = 4 + 31 * 2; // 66

    QStringList headers;
    headers.reserve(totalCols);
    headers << QStringLiteral("row_id") << QStringLiteral("Product") << QStringLiteral("Cost") << QStringLiteral("Min Stock");
    for (int day = 1; day <= 31; ++day) {
        headers.append(QStringLiteral("%1.%2").arg(day).arg(monthPad));
        headers.append(QStringLiteral("%1.%2 P").arg(day).arg(monthPad));
    }

    // Reset table
    table.clearSpans();
    table.clear();
    table.setRowCount(0);
    table.setColumnCount(totalCols);
    table.setHorizontalHeaderLabels(headers);

    // Column visibility and widths
    table.setColumnHidden(0, true);
    table.setColumnWidth(1, 180);
    table.setColumnWidth(2, 70);
    table.setColumnWidth(3, 70);
    table.horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table.horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table.horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);

    for (int day = 1; day <= 31; ++day) {
        const int qtyCol = 4 + (day - 1) * 2;
        const int purchCol = 4 + (day - 1) * 2 + 1;
        const bool visible = (day <= visibleDays);
        table.setColumnHidden(qtyCol, !visible);
        table.setColumnHidden(purchCol, !visible);
        if (visible) {
            table.setColumnWidth(qtyCol, 58);
            table.setColumnWidth(purchCol, 50);
        }
    }

    // Load data
    QJsonArray rows;
    try {
        rows = context_.services().inventory->loadSnapshot(financialYear, month);
    } catch (const std::exception& e) {
        showError(tr("Failed to load inventory: %1").arg(e.what()), e);
        table.horizontalHeader()->resizeSections(QHeaderView::Fixed);
        return;
    }

    if (rows.isEmpty()) {
        table.setRowCount(1);
        auto* placeholder = new QTableWidgetItem(tr("No inventory rows yet..."));
        placeholder->setFlags(Qt::ItemIsEnabled);
        placeholder->setForeground(QColor(QStringLiteral("#58677a")));
        table.setItem(0, 1, placeholder);
        table.setSpan(0, 1, 1, totalCols - 2);
        table.horizontalHeader()->resizeSections(QHeaderView::Fixed);
        return;
    }

    // Populate rows
    const int today = QDate::currentDate().day();
    const QColor highlightBlueBg(QStringLiteral("#dbeafe"));
    const QColor highlightBlueFg(QStringLiteral("#1e3a8a"));
    const QColor reorderBg(QStringLiteral("#fef2f2"));
    const QColor reorderFg(QStringLiteral("#b91c1c"));
    const QColor purchaseGreenBg(QStringLiteral("#ecfdf3"));
    const QColor purchaseGreenFg(QStringLiteral("#047857"));

    table.setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const QJsonObject row = rows[r].toObject();

        auto* idItem = new QTableWidgetItem(row[QStringLiteral("row_id")].toVariant().toString());
        idItem->setFlags(Qt::ItemIsEnabled);
        table.setItem(r, 0, idItem);

        auto* nameItem = new QTableWidgetItem(row[QStringLiteral("name")].toString());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        table.setItem(r, 1, nameItem);

        auto* costItem = new QTableWidgetItem(row[QStringLiteral("cost")].toVariant().toString());
        costItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        costItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table.setItem(r, 2, costItem);

        auto* minItem = new QTableWidgetItem(row[QStringLiteral("min_stock")].toVariant().toString());
        minItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        minItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table.setItem(r, 3, minItem);

        double currentQty = 0.0;
        const double minStock = row[QStringLiteral("min_stock")].toDouble();

        for (int day = 1; day <= 31; ++day) {
            const int qtyCol = 4 + (day - 1) * 2;
            const int purchCol = 4 + (day - 1) * 2 + 1;
            const bool isCurrent = (day == today && day <= visibleDays);

            // Qty cell
            const QString qtyKey = QString::asprintf("qty_%02d", day);
            const double qty = row[qtyKey].toDouble();
            const QString qtyText = (qty == 0.0) ? QString{} : QString::number(qty, 'f', 2);
            auto* qtyItem = new QTableWidgetItem(qtyText);
            qtyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (isCurrent) {
                qtyItem->setBackground(highlightBlueBg);
                qtyItem->setForeground(highlightBlueFg);
                QFont bf = qtyItem->font(); bf.setBold(true); qtyItem->setFont(bf);
            }
            if (day == today) currentQty = qty;
            table.setItem(r, qtyCol, qtyItem);

            // Purchase cell
            const QString purchKey = QString::asprintf("purchase_%02d", day);
            const double purchase = row[purchKey].toDouble();
            const QString purchText = (purchase == 0.0) ? QString{} : QString::number(purchase, 'f', 2);
            auto* purchItem = new QTableWidgetItem(purchText);
            purchItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
            purchItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (purchase > 0.0) {
                purchItem->setBackground(purchaseGreenBg);
                purchItem->setForeground(purchaseGreenFg);
            }
            if (isCurrent) {
                purchItem->setBackground(highlightBlueBg);
                purchItem->setForeground(highlightBlueFg);
            }
            table.setItem(r, purchCol, purchItem);
        }

        // Reorder highlighting
        const bool isReorder = minStock > 0.0 && currentQty < minStock;
        if (isReorder) {
            nameItem->setBackground(reorderBg);
            nameItem->setForeground(reorderFg);
            QFont bf = nameItem->font(); bf.setBold(true); nameItem->setFont(bf);
            costItem->setBackground(reorderBg);
            costItem->setForeground(reorderFg);
            minItem->setBackground(reorderBg);
            minItem->setForeground(reorderFg);
        }
    }

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
        headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("particulars"), QStringLiteral("debit"), QStringLiteral("credit"), QStringLiteral("balance")};
    } else if (reportName == QStringLiteral("Day Book")) {
        headers = {QStringLiteral("date"), QStringLiteral("bill_no"), QStringLiteral("party"), QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Purchase Report")) {
        headers = {QStringLiteral("section"), QStringLiteral("name"), QStringLiteral("total_purchase")};
    } else if (reportName == QStringLiteral("Expenses")) {
        headers = {QStringLiteral("date"), QStringLiteral("party"), QStringLiteral("mode"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Outstanding")) {
        headers = {QStringLiteral("party"), QStringLiteral("status"), QStringLiteral("days_unpaid"), QStringLiteral("last_receipt"), QStringLiteral("balance")};
    } else if (reportName == QStringLiteral("Trial Balance")) {
        headers = {QStringLiteral("account"), QStringLiteral("debit"), QStringLiteral("credit")};
    } else if (reportName == QStringLiteral("Profit & Loss")) {
        headers = {QStringLiteral("metric"), QStringLiteral("amount")};
    } else if (reportName == QStringLiteral("Daily Summary")) {
        headers = {
            QStringLiteral("date"),
            QStringLiteral("opening_cash"),
            QStringLiteral("cash_in"),
            QStringLiteral("cash_expense"),
            QStringLiteral("cash_needed"),
            QStringLiteral("cash_in_hand"),
            QStringLiteral("cash_short_excess"),
            QStringLiteral("bank"),
            QStringLiteral("credit_sale"),
            QStringLiteral("total_sales")
        };
    } else if (reportName == QStringLiteral("Short / Excess")) {
        headers = {
            QStringLiteral("date"),
            QStringLiteral("opening_cash"),
            QStringLiteral("cash_in"),
            QStringLiteral("cash_expense"),
            QStringLiteral("cash_needed"),
            QStringLiteral("cash_in_hand"),
            QStringLiteral("cash_short_excess")
        };
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
                placeholder.insert(QStringLiteral("particulars"), QStringLiteral("Enter a party name and refresh"));
                placeholder.insert(QStringLiteral("debit"), QStringLiteral(""));
                placeholder.insert(QStringLiteral("credit"), QStringLiteral(""));
                placeholder.insert(QStringLiteral("balance"), QStringLiteral("--"));
                rows.append(placeholder);
                setTableRows(table, headers, rows);
                return;
            }
            const QJsonObject report = context_.services().reports->ledger(partyName, range);
            const QJsonArray rawData = report.value(QStringLiteral("data")).toArray();
            // Transform sale/purchase rows into debit and receipt/return rows into credit.
            QJsonArray ledgerRows;
            for (const QJsonValue& val : rawData) {
                const QJsonObject raw = val.toObject();
                const QString type = raw.value(QStringLiteral("type")).toString().trimmed().toLower();
                const QString mode = raw.value(QStringLiteral("mode")).toString().trimmed();
                const double amount = raw.value(QStringLiteral("amount")).toDouble();
                const double balance = raw.value(QStringLiteral("balance")).toDouble();
                const QString particulars = raw.value(QStringLiteral("type")).toString()
                    + (mode.isEmpty() ? QString() : QStringLiteral(" / %1").arg(mode));
                QString debit;
                QString credit;
                if (type == QStringLiteral("sale") || type == QStringLiteral("expense") || type == QStringLiteral("purchase")) {
                    debit = moneyText(amount);
                } else {
                    credit = moneyText(amount);
                }
                const QString balanceText = balance < 0
                    ? QStringLiteral("%1 Cr").arg(moneyText(qAbs(balance)))
                    : QStringLiteral("%1 Dr").arg(moneyText(balance));
                QJsonObject row;
                row.insert(QStringLiteral("date"), raw.value(QStringLiteral("date")));
                row.insert(QStringLiteral("bill_no"), raw.value(QStringLiteral("bill_no")));
                row.insert(QStringLiteral("particulars"), particulars);
                row.insert(QStringLiteral("debit"), debit);
                row.insert(QStringLiteral("credit"), credit);
                row.insert(QStringLiteral("balance"), balanceText);
                ledgerRows.append(row);
            }
            setTableRows(table, headers, ledgerRows);
            return;
        }
        if (reportName == QStringLiteral("Day Book")) {
            setTableRows(table, headers, context_.services().reports->dayBook(range.end));
            return;
        }
        if (reportName == QStringLiteral("Purchase Report")) {
            const int days = static_cast<int>(std::max<qint64>(1, range.start.daysTo(QDate::currentDate()) + 1));
            const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 1000, days);
            QMap<QString, double> monthlyTotals;
            QMap<QString, double> partyTotals;
            for (const domain::TransactionRow& row : rows) {
                if (row.date < range.start || row.date > range.end) {
                    continue;
                }
                if (transactionTypeText(row.type) != QStringLiteral("Purchase")) {
                    continue;
                }
                monthlyTotals[row.date.toString(QStringLiteral("yyyy-MM"))] += row.amount;
                partyTotals[row.party] += row.amount;
            }
            QJsonArray data;
            for (auto it = monthlyTotals.cbegin(); it != monthlyTotals.cend(); ++it) {
                QJsonObject item;
                item.insert(QStringLiteral("section"), QStringLiteral("Monthly"));
                item.insert(QStringLiteral("name"), it.key());
                item.insert(QStringLiteral("total_purchase"), it.value());
                data.append(item);
            }
            for (auto it = partyTotals.cbegin(); it != partyTotals.cend(); ++it) {
                QJsonObject item;
                item.insert(QStringLiteral("section"), QStringLiteral("Party"));
                item.insert(QStringLiteral("name"), it.key());
                item.insert(QStringLiteral("total_purchase"), it.value());
                data.append(item);
            }
            setTableRows(table, headers, data);
            return;
        }
        if (reportName == QStringLiteral("Expenses")) {
            const int days = static_cast<int>(std::max<qint64>(1, range.start.daysTo(QDate::currentDate()) + 1));
            const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 1000, days);
            QJsonArray data;
            for (const domain::TransactionRow& row : rows) {
                if (row.date < range.start || row.date > range.end || transactionTypeText(row.type) != QStringLiteral("Expense")) {
                    continue;
                }
                QJsonObject item;
                item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
                item.insert(QStringLiteral("party"), row.party);
                item.insert(QStringLiteral("mode"), paymentModeText(row.mode));
                item.insert(QStringLiteral("amount"), row.amount);
                data.append(item);
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
            QJsonArray rows = context_.services().reports->shortExcess(range);
            double totalShortExcess = 0.0;
            for (const QJsonValue& value : rows) {
                totalShortExcess += value.toObject().value(QStringLiteral("cash_short_excess")).toDouble();
            }
            QJsonObject total;
            total.insert(QStringLiteral("date"), QStringLiteral("Total"));
            total.insert(QStringLiteral("opening_cash"), QStringLiteral(""));
            total.insert(QStringLiteral("cash_in"), QStringLiteral(""));
            total.insert(QStringLiteral("cash_expense"), QStringLiteral(""));
            total.insert(QStringLiteral("cash_needed"), QStringLiteral(""));
            total.insert(QStringLiteral("cash_in_hand"), QStringLiteral(""));
            total.insert(QStringLiteral("cash_short_excess"), totalShortExcess);
            rows.append(total);
            setTableRows(table, headers, rows);
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

int DesktopApplication::importTransactionsFromCsv(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QStringLiteral("Could not open CSV file: %1").arg(path).toStdString());
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    if (stream.atEnd()) {
        throw std::runtime_error(QStringLiteral("CSV file is empty: %1").arg(path).toStdString());
    }

    const QStringList header = parseCsvLine(stream.readLine());
    QMap<QString, int> columns;
    for (int index = 0; index < header.size(); index += 1) {
        columns.insert(header.at(index).trimmed().toLower(), index);
    }

    const QStringList required = {
        QStringLiteral("date"),
        QStringLiteral("bill_no"),
        QStringLiteral("party"),
        QStringLiteral("type"),
        QStringLiteral("mode"),
        QStringLiteral("amount")
    };
    for (const QString& key : required) {
        if (!columns.contains(key)) {
            throw std::runtime_error(QStringLiteral("CSV file %1 is missing required column: %2").arg(path, key).toStdString());
        }
    }

    const QStringList existingParties = partyNames();
    int imported = 0;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        lineNumber += 1;
        const QString rawLine = stream.readLine();
        if (rawLine.trimmed().isEmpty()) {
            continue;
        }
        const QStringList values = parseCsvLine(rawLine);
        auto valueAt = [values, columns, lineNumber, path](const QString& key) {
            const int column = columns.value(key);
            if (column < 0 || column >= values.size()) {
                throw std::runtime_error(QStringLiteral("CSV file %1 line %2 is missing value for column: %3").arg(path).arg(lineNumber).arg(key).toStdString());
            }
            return values.at(column).trimmed();
        };

        const QDate date = QDate::fromString(valueAt(QStringLiteral("date")), Qt::ISODate);
        if (!date.isValid()) {
            throw std::runtime_error(QStringLiteral("CSV file %1 line %2 has invalid ISO date").arg(path).arg(lineNumber).toStdString());
        }
        const QString partyName = findExistingPartyName(existingParties, valueAt(QStringLiteral("party")));
        if (partyName.isEmpty()) {
            throw std::runtime_error(QStringLiteral("CSV file %1 line %2 references a party that does not exist").arg(path).arg(lineNumber).toStdString());
        }
        bool amountOk = false;
        const double amount = valueAt(QStringLiteral("amount")).toDouble(&amountOk);
        if (!amountOk || amount <= 0.0) {
            throw std::runtime_error(QStringLiteral("CSV file %1 line %2 has invalid amount").arg(path).arg(lineNumber).toStdString());
        }

        context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
            date,
            valueAt(QStringLiteral("bill_no")),
            partyName,
            transactionTypeFromText(valueAt(QStringLiteral("type"))),
            paymentModeFromText(valueAt(QStringLiteral("mode"))),
            amount
        });
        imported += 1;
    }

    return imported;
}

struct ImportRowPreview final {
    int line;
    QString date;
    QString billNo;
    QString party;
    QString type;
    QString mode;
    double amount;
    QString error;
};

void DesktopApplication::importTransactionsWithValidation() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Transactions"),
        QString(),
        QStringLiteral("CSV Files (*.csv);;Excel Files (*.xlsx *.xls)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    QVector<ImportRowPreview> previewRows;
    const QStringList existingParties = partyNames();
    int maxLine = 0;

    if (path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive) ||
        path.endsWith(QStringLiteral(".xls"), Qt::CaseInsensitive)) {
        const app::XlsxParseResult parsed = app::XlsxReader::read(path);
        if (!parsed.error.isEmpty()) {
            throw std::runtime_error(parsed.error.toStdString());
        }
        for (int i = 0; i < parsed.rows.size(); ++i) {
            const auto& row = parsed.rows.at(i);
            ImportRowPreview preview;
            preview.line = i + 2;
            preview.date = row.date.isValid() ? row.date.toString(Qt::ISODate) : row.date.toString(QStringLiteral("dd/MM/yyyy"));
            preview.billNo = row.billNo;
            preview.party = row.party;
            preview.type = row.type;
            preview.mode = row.mode;
            preview.amount = row.amount;

            QStringList errors;
            if (!row.date.isValid()) {
                errors.append(QStringLiteral("Invalid date"));
            }
            if (row.party.trimmed().isEmpty()) {
                errors.append(QStringLiteral("Party is empty"));
            } else if (findExistingPartyName(existingParties, row.party).isEmpty()) {
                errors.append(QStringLiteral("Party does not exist: ") + row.party);
            }
            if (row.amount <= 0.0) {
                errors.append(QStringLiteral("Amount must be > 0"));
            }
            const QString normalizedType = row.type.trimmed().toLower();
            if (normalizedType != QStringLiteral("sale") && normalizedType != QStringLiteral("sale return") &&
                normalizedType != QStringLiteral("purchase") && normalizedType != QStringLiteral("expense") &&
                normalizedType != QStringLiteral("receipt") && normalizedType != QStringLiteral("reciept") &&
                normalizedType != QStringLiteral("sales return") && normalizedType != QStringLiteral("return")) {
                errors.append(QStringLiteral("Invalid type: ") + row.type);
            }
            const QString normalizedMode = row.mode.trimmed().toLower();
            if (normalizedMode != QStringLiteral("credit") && normalizedMode != QStringLiteral("cash") &&
                normalizedMode != QStringLiteral("upi") && normalizedMode != QStringLiteral("gpay") &&
                normalizedMode != QStringLiteral("bank")) {
                errors.append(QStringLiteral("Invalid mode: ") + row.mode);
            }
            preview.error = errors.isEmpty() ? QString() : errors.join(QStringLiteral("; "));
            previewRows.append(preview);
        }
        maxLine = parsed.rows.size() + 1;
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            throw std::runtime_error(QStringLiteral("Could not open file: %1").arg(path).toStdString());
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        if (stream.atEnd()) {
            throw std::runtime_error(QStringLiteral("File is empty: %1").arg(path).toStdString());
        }

        const QStringList header = parseCsvLine(stream.readLine());
        QMap<QString, int> columns;
        for (int i = 0; i < header.size(); ++i) {
            const QString key = header.at(i).trimmed().toLower();
            columns.insert(key, i);
            if (key == QStringLiteral("bill no") || key == QStringLiteral("billno")) {
                columns.insert(QStringLiteral("bill_no"), i);
            } else if (key == QStringLiteral("bill_no")) {
                columns.insert(QStringLiteral("bill no"), i);
                columns.insert(QStringLiteral("billno"), i);
            } else if (key == QStringLiteral("txn date") || key == QStringLiteral("transaction date")) {
                columns.insert(QStringLiteral("date"), i);
            }
        }
        const QStringList required = {
            QStringLiteral("date"), QStringLiteral("party"),
            QStringLiteral("type"), QStringLiteral("mode"), QStringLiteral("amount")
        };
        for (const QString& key : required) {
            if (!columns.contains(key)) {
                throw std::runtime_error(QStringLiteral("CSV file is missing required column: %1").arg(key).toStdString());
            }
        }

        int lineNumber = 1;
        while (!stream.atEnd()) {
            lineNumber += 1;
            const QString rawLine = stream.readLine();
            if (rawLine.trimmed().isEmpty()) continue;

            const QStringList values = parseCsvLine(rawLine);
            auto val = [&](const QString& key) -> QString {
                int col = columns.value(key, -1);
                return (col >= 0 && col < values.size()) ? values.at(col).trimmed() : QString();
            };

            ImportRowPreview preview;
            preview.line = lineNumber;
            preview.date = val(QStringLiteral("date"));
            preview.billNo = val(QStringLiteral("bill_no"));
            preview.party = val(QStringLiteral("party"));
            preview.type = val(QStringLiteral("type"));
            preview.mode = val(QStringLiteral("mode"));

            bool amountOk = false;
            preview.amount = val(QStringLiteral("amount")).toDouble(&amountOk);

            const QString dateText = val(QStringLiteral("date"));
            QDate date = QDate::fromString(dateText, Qt::ISODate);
            if (!date.isValid()) {
                date = QDate::fromString(dateText, QStringLiteral("dd/MM/yyyy"));
            }
            if (!date.isValid()) {
                date = QDate::fromString(dateText, QStringLiteral("dd-MM-yyyy"));
            }

            QStringList errors;
            if (date.isValid()) {
                preview.date = date.toString(Qt::ISODate);
            } else {
                errors.append(QStringLiteral("Invalid date"));
            }
            if (preview.party.isEmpty()) {
                errors.append(QStringLiteral("Party is empty"));
            } else if (findExistingPartyName(existingParties, preview.party).isEmpty()) {
                errors.append(QStringLiteral("Party does not exist: ") + preview.party);
            }
            if (!amountOk || preview.amount <= 0.0) {
                errors.append(QStringLiteral("Amount must be > 0"));
            }
            const QString nt = preview.type.trimmed().toLower();
            if (nt != QStringLiteral("sale") && nt != QStringLiteral("sale return") &&
                nt != QStringLiteral("purchase") && nt != QStringLiteral("expense") &&
                nt != QStringLiteral("receipt") && nt != QStringLiteral("reciept") &&
                nt != QStringLiteral("sales return") && nt != QStringLiteral("return")) {
                errors.append(QStringLiteral("Invalid type: ") + preview.type);
            }
            const QString nm = preview.mode.trimmed().toLower();
            if (nm != QStringLiteral("credit") && nm != QStringLiteral("cash") &&
                nm != QStringLiteral("upi") && nm != QStringLiteral("gpay") &&
                nm != QStringLiteral("bank")) {
                errors.append(QStringLiteral("Invalid mode: ") + preview.mode);
            }
            preview.error = errors.isEmpty() ? QString() : errors.join(QStringLiteral("; "));
            previewRows.append(preview);
        }
        maxLine = lineNumber;
    }

    int validCount = 0;
    int invalidCount = 0;
    for (const auto& p : previewRows) {
        if (p.error.isEmpty()) ++validCount;
        else ++invalidCount;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Import Preview"));
    dialog.resize(640, 480);
    dialog.setMinimumSize(480, 320);

    QVBoxLayout* dlgLayout = new QVBoxLayout(&dialog);
    dlgLayout->setSpacing(10);

    QLabel* summary = new QLabel(
        QStringLiteral("Found %1 rows (%2 valid, %3 invalid). Review and confirm to import.")
            .arg(previewRows.size()).arg(validCount).arg(invalidCount), &dialog);
    summary->setWordWrap(true);
    dlgLayout->addWidget(summary);

    QTableWidget* previewTable = new QTableWidget(&dialog);
    QStringList tableHeaders = {
        QStringLiteral("Line"), QStringLiteral("Date"), QStringLiteral("Bill No"),
        QStringLiteral("Party"), QStringLiteral("Type"), QStringLiteral("Mode"),
        QStringLiteral("Amount"), QStringLiteral("Errors")
    };
    previewTable->setColumnCount(tableHeaders.size());
    previewTable->setHorizontalHeaderLabels(tableHeaders);
    previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    previewTable->setRowCount(previewRows.size());

    for (int row = 0; row < previewRows.size(); ++row) {
        const auto& p = previewRows.at(row);
        auto setCell = [&](int col, const QString& text) {
            QTableWidgetItem* item = new QTableWidgetItem(text);
            if (!p.error.isEmpty()) {
                item->setForeground(QColor(Qt::red));
            }
            previewTable->setItem(row, col, item);
        };
        setCell(0, QString::number(p.line));
        setCell(1, p.date);
        setCell(2, p.billNo);
        setCell(3, p.party);
        setCell(4, p.type);
        setCell(5, p.mode);
        setCell(6, QString::number(p.amount, 'f', 2));
        setCell(7, p.error);
    }
    previewTable->resizeColumnsToContents();
    dlgLayout->addWidget(previewTable, 1);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);
    QPushButton* cancelBtn = new QPushButton(QStringLiteral("Cancel"), &dialog);
    QPushButton* importBtn = new QPushButton(
        QStringLiteral("Import %1 Valid Transactions").arg(validCount), &dialog);
    importBtn->setObjectName(QStringLiteral("primaryButton"));
    if (validCount == 0) {
        importBtn->setEnabled(false);
    }
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(importBtn);
    dlgLayout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(importBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    int imported = 0;
    const QString adminUser = QStringLiteral("native-admin");
    for (const auto& p : previewRows) {
        if (!p.error.isEmpty()) continue;

        QDate date = QDate::fromString(p.date, Qt::ISODate);
        if (!date.isValid()) {
            date = QDate::fromString(p.date, QStringLiteral("dd/MM/yyyy"));
        }
        const QString partyName = findExistingPartyName(existingParties, p.party);

        context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
            date,
            p.billNo,
            partyName,
            transactionTypeFromText(p.type),
            paymentModeFromText(p.mode),
            p.amount
        });
        ++imported;
    }

    statusBar()->showMessage(
        QStringLiteral("Imported %1 of %2 valid transactions (%3 skipped)")
            .arg(imported).arg(validCount).arg(invalidCount), 10000);
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
            const int qtyColumn = 4 + (day - 1) * 2;
            const int purchaseColumn = 4 + (day - 1) * 2 + 1;
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
                text = isMoneyColumn(key) ? moneyText(value.toDouble()) : QString::number(value.toDouble(), 'f', 2);
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

void DesktopApplication::exportTableToExcel(QTableWidget& table, const QString& title) {
    const QString defaultName = QStringLiteral("%1.csv").arg(title.trimmed().isEmpty() ? QStringLiteral("M-Finlogs") : title).replace(QLatin1Char('/'), QLatin1Char('-'));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Excel"), defaultName, QStringLiteral("Excel-compatible CSV (*.csv)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        throw std::runtime_error(QStringLiteral("Could not write export file: %1").arg(file.errorString()).toStdString());
    }

    auto csvCell = [](const QString& value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(escaped);
    };

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QStringList headerCells;
    for (int column = 0; column < table.columnCount(); column += 1) {
        if (!table.isColumnHidden(column)) {
            const QString label = table.horizontalHeaderItem(column) ? table.horizontalHeaderItem(column)->text() : QString::number(column + 1);
            headerCells.append(csvCell(label));
        }
    }
    stream << headerCells.join(QLatin1Char(',')) << Qt::endl;

    for (int row = 0; row < table.rowCount(); row += 1) {
        if (table.isRowHidden(row)) {
            continue;
        }
        QStringList cells;
        for (int column = 0; column < table.columnCount(); column += 1) {
            if (!table.isColumnHidden(column)) {
                const QTableWidgetItem* item = table.item(row, column);
                cells.append(csvCell(item ? item->text() : QString()));
            }
        }
        stream << cells.join(QLatin1Char(',')) << Qt::endl;
    }

    statusBar()->showMessage(QStringLiteral("Excel export created: %1").arg(path), 8000);
}

void DesktopApplication::exportTableToPdf(QTableWidget& table, const QString& title) {
    const QString defaultName = QStringLiteral("%1.pdf").arg(title.trimmed().isEmpty() ? QStringLiteral("M-Finlogs") : title).replace(QLatin1Char('/'), QLatin1Char('-'));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export PDF"), defaultName, QStringLiteral("PDF Files (*.pdf)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    QVector<int> visibleColumns;
    for (int column = 0; column < table.columnCount(); column += 1) {
        if (!table.isColumnHidden(column)) {
            visibleColumns.append(column);
        }
    }
    QVector<int> visibleRows;
    for (int row = 0; row < table.rowCount(); row += 1) {
        if (!table.isRowHidden(row)) {
            visibleRows.append(row);
        }
    }
    if (visibleColumns.isEmpty() || visibleRows.isEmpty()) {
        throw std::runtime_error("No visible table data to export");
    }

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setResolution(96);
    writer.setTitle(title);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        throw std::runtime_error(QStringLiteral("Could not create PDF: %1").arg(path).toStdString());
    }

    const int pageWidth = writer.width();
    const int pageHeight = writer.height();
    const int margin = 28;
    const int titleHeight = 32;
    const int rowHeight = 22;
    const int headerHeight = 24;
    const int tableWidth = pageWidth - margin * 2;
    const int columnsPerPage = qMax(1, tableWidth / 110);

    auto drawHeader = [&](int& y, const QVector<int>& pageColumns, int columnWidth) {
        painter.setPen(QColor(QStringLiteral("#111827")));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 15, QFont::Bold));
        painter.drawText(QRect(margin, y, tableWidth, titleHeight), Qt::AlignLeft | Qt::AlignVCenter, title);
        y += titleHeight;

        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::Bold));
        painter.fillRect(QRect(margin, y, tableWidth, headerHeight), QColor(QStringLiteral("#eef2f7")));
        painter.setPen(QColor(QStringLiteral("#374151")));
        int x = margin;
        for (int column : pageColumns) {
            const QString label = table.horizontalHeaderItem(column) ? table.horizontalHeaderItem(column)->text() : QString::number(column + 1);
            painter.drawText(QRect(x + 4, y, columnWidth - 8, headerHeight), Qt::AlignLeft | Qt::AlignVCenter, label);
            x += columnWidth;
        }
        painter.setPen(QColor(QStringLiteral("#cbd5e1")));
        painter.drawRect(QRect(margin, y, tableWidth, headerHeight));
        y += headerHeight;
    };

    bool firstPage = true;
    for (int columnStart = 0; columnStart < visibleColumns.size(); columnStart += columnsPerPage) {
        QVector<int> pageColumns;
        for (int offset = 0; offset < columnsPerPage && columnStart + offset < visibleColumns.size(); offset += 1) {
            pageColumns.append(visibleColumns.at(columnStart + offset));
        }
        const int columnWidth = qMax(1, tableWidth / pageColumns.size());
        if (!firstPage) {
            writer.newPage();
        }
        firstPage = false;

        int y = margin;
        drawHeader(y, pageColumns, columnWidth);
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        for (int row : visibleRows) {
            if (y + rowHeight > pageHeight - margin) {
                writer.newPage();
                y = margin;
                drawHeader(y, pageColumns, columnWidth);
                painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
            }

            if (row % 2 == 1) {
                painter.fillRect(QRect(margin, y, tableWidth, rowHeight), QColor(QStringLiteral("#f8fafc")));
            }
            int x = margin;
            for (int column : pageColumns) {
                const QTableWidgetItem* item = table.item(row, column);
                const QString text = item ? item->text() : QString();
                painter.setPen(QColor(QStringLiteral("#111827")));
                painter.drawText(QRect(x + 4, y, columnWidth - 8, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, painter.fontMetrics().elidedText(text, Qt::ElideRight, columnWidth - 8));
                painter.setPen(QColor(QStringLiteral("#e5e7eb")));
                painter.drawRect(QRect(x, y, columnWidth, rowHeight));
                x += columnWidth;
            }
            y += rowHeight;
        }
    }
    painter.end();
    statusBar()->showMessage(QStringLiteral("PDF exported: %1").arg(path), 8000);
}

void DesktopApplication::printTable(QTableWidget& table, const QString& title) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageOrientation(QPageLayout::Landscape);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Print %1").arg(title));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter(&printer);
    if (!painter.isActive()) {
        throw std::runtime_error("Could not start printer painter");
    }

    const QRect page = printer.pageRect(QPrinter::DevicePixel).toRect();
    const int margin = 80;
    const int rowHeight = 42;
    const int headerHeight = 48;
    const int tableWidth = page.width() - margin * 2;
    QVector<int> visibleColumns;
    for (int column = 0; column < table.columnCount(); column += 1) {
        if (!table.isColumnHidden(column)) {
            visibleColumns.append(column);
        }
    }
    if (visibleColumns.isEmpty()) {
        throw std::runtime_error("No visible table columns to print");
    }
    const int columnWidth = qMax(1, tableWidth / visibleColumns.size());
    int y = margin;
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 14, QFont::Bold));
    painter.drawText(QRect(margin, y, tableWidth, headerHeight), Qt::AlignLeft | Qt::AlignVCenter, title);
    y += headerHeight;

    auto drawHeader = [&]() {
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 8, QFont::Bold));
        int x = margin;
        for (int column : visibleColumns) {
            const QString label = table.horizontalHeaderItem(column) ? table.horizontalHeaderItem(column)->text() : QString::number(column + 1);
            painter.drawText(QRect(x, y, columnWidth, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, label);
            x += columnWidth;
        }
        y += rowHeight;
    };
    drawHeader();
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
    for (int row = 0; row < table.rowCount(); row += 1) {
        if (table.isRowHidden(row)) {
            continue;
        }
        if (y + rowHeight > page.height() - margin) {
            printer.newPage();
            y = margin;
            drawHeader();
            painter.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        }
        int x = margin;
        for (int column : visibleColumns) {
            const QTableWidgetItem* item = table.item(row, column);
            painter.drawText(QRect(x, y, columnWidth, rowHeight), Qt::AlignLeft | Qt::AlignVCenter, item ? item->text() : QString());
            x += columnWidth;
        }
        y += rowHeight;
    }
}

QWidget* DesktopApplication::ensurePageBuilt(int index) {
    if (index == 0) return pages_->widget(0);
    if (builtPages_.contains(index)) return pages_->widget(index);
    const int factoryIdx = index - 1;
    if (factoryIdx < 0 || factoryIdx >= pageFactories_.size()) return nullptr;
    QWidget* page = pageFactories_[factoryIdx]();
    if (!page) return nullptr;
    pages_->removeWidget(pages_->widget(index));
    pages_->insertWidget(index, page);
    builtPages_.insert(index);
    return page;
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
    QAction* logoutAction = toolbar->addAction(QStringLiteral("Logout"));
    updateAction->setIcon(appIcon(QStringLiteral("refresh")));
    backupAction->setIcon(appIcon(QStringLiteral("database")));

    connect(logoutAction, &QAction::triggered, this, [this]() {
        const QMessageBox::StandardButton result = QMessageBox::question(
            this, QStringLiteral("Logout"), QStringLiteral("Sign out and close the application?"),
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            QApplication::quit();
        }
    });

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

    // Show the modern frameless branded splash first.
    auto splash = std::make_unique<SplashScreen>();
    std::unique_ptr<AppContext> context;
    std::unique_ptr<DesktopApplication> window;

    splash->show();
    splash->setProgress(10, QStringLiteral("Loading runtime..."));

    // Simulate staged loading with status messages
    QTimer::singleShot(300, splash.get(), [&splash]() {
        splash->setProgress(25, QStringLiteral("Connecting SQL Server..."));
    });
    QTimer::singleShot(600, splash.get(), [&splash]() {
        splash->setProgress(50, QStringLiteral("Loading workspace configuration..."));
    });
    QTimer::singleShot(900, splash.get(), [&splash]() {
        splash->setProgress(75, QStringLiteral("Preparing interface..."));
    });

    QObject::connect(splash.get(), &SplashScreen::finished, &qtApp, [&]() {
        splash->close();
        try {
            context = std::make_unique<AppContext>(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
            window = std::make_unique<DesktopApplication>(*context);
            window->show();
            window->raise();
            window->activateWindow();
        } catch (const std::exception& err) {
            QMessageBox::critical(nullptr,
                QStringLiteral("M-Finlogs Native"),
                QStringLiteral("Native app startup failed.\n\n%1").arg(QString::fromUtf8(err.what())));
            QTimer::singleShot(0, &qtApp, &QApplication::quit);
        }
    });
    splash->runIndeterminate(1600);

    return qtApp.exec();
}

} // namespace mfinlogs::app
