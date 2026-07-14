#include "app/QmlBackend.h"
#include "app/XlsxReader.h"

#include "data/MigrationService.h"
#include "domain/BalanceCalculator.h"
#include "domain/DomainErrors.h"
#include "domain/Types.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVector>

#include <QThreadPool>
#include <QRunnable>
#include <QElapsedTimer>
#include <QPointer>

#include <exception>

namespace mfinlogs::app {

namespace {

// ---- JSON <-> QVariant conversion ---------------------------------------

QVariant jsonValueToVariant(const QJsonValue& value);

QVariantMap jsonObjectToMap(const QJsonObject& object) {
    QVariantMap map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        map.insert(it.key(), jsonValueToVariant(it.value()));
    }
    return map;
}

QVariantList jsonArrayToList(const QJsonArray& array) {
    QVariantList list;
    list.reserve(array.size());
    for (const QJsonValue& value : array) {
        list.append(jsonValueToVariant(value));
    }
    return list;
}

QVariant jsonValueToVariant(const QJsonValue& value) {
    switch (value.type()) {
    case QJsonValue::Object:
        return jsonObjectToMap(value.toObject());
    case QJsonValue::Array:
        return jsonArrayToList(value.toArray());
    case QJsonValue::Bool:
        return value.toBool();
    case QJsonValue::Double:
        return value.toDouble();
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return QVariant();
    }
}

QVariantList partyBalancesFromOutstanding(const QJsonObject& outstanding) {
    QVariantList result;
    const QJsonArray rows = outstanding.value(QStringLiteral("data")).toArray();
    result.reserve(rows.size());
    for (const QJsonValue& value : rows) {
        const QJsonObject row = value.toObject();
        // closing_balance is the full historical balance (lifetime debit - credit)
        const double balance = row.value(QStringLiteral("closing_balance")).toDouble(
            row.value(QStringLiteral("balance")).toDouble());
        QVariantMap entry = jsonObjectToMap(row);
        entry.insert(QStringLiteral("balance"), balance);
        entry.insert(QStringLiteral("closing_balance"), balance);
        entry.insert(QStringLiteral("debit"), row.value(QStringLiteral("debit")).toDouble());
        entry.insert(QStringLiteral("credit"), row.value(QStringLiteral("credit")).toDouble());
        entry.insert(QStringLiteral("transaction_count"), row.value(QStringLiteral("transaction_count")).toInt());
        entry.insert(QStringLiteral("last_transaction_date"), row.value(QStringLiteral("last_transaction_date")).toString());
        entry.insert(QStringLiteral("balanceLabel"), QLocale().toString(qAbs(balance), 'f', 2)
            + (balance >= 0.0 ? QStringLiteral(" Dr") : QStringLiteral(" Cr")));
        entry.insert(QStringLiteral("lastDate"), row.value(QStringLiteral("last_receipt")).toString());
        entry.insert(QStringLiteral("lastType"), QStringLiteral("Receipt"));
        entry.insert(QStringLiteral("lastAmount"), 0.0);
        result.append(entry);
    }
    return result;
}

// ---- Enum mapping (mirrors DesktopApplication helpers) -------------------

domain::TransactionType transactionTypeFromText(const QString& text) {
    if (text == QStringLiteral("Sale")) {
        return domain::TransactionType::Sale;
    }
    if (text == QStringLiteral("Sale Return")) {
        return domain::TransactionType::SaleReturn;
    }
    if (text == QStringLiteral("Purchase")) {
        return domain::TransactionType::Purchase;
    }
    if (text == QStringLiteral("Expense")) {
        return domain::TransactionType::Expense;
    }
    return domain::TransactionType::Receipt;
}

domain::PaymentMode paymentModeFromText(const QString& text) {
    if (text == QStringLiteral("Credit")) {
        return domain::PaymentMode::Credit;
    }
    if (text == QStringLiteral("UPI")) {
        return domain::PaymentMode::Upi;
    }
    if (text == QStringLiteral("Bank")) {
        return domain::PaymentMode::Bank;
    }
    return domain::PaymentMode::Cash;
}

QString roleToText(domain::UserRole role) {
    return role == domain::UserRole::Admin ? QStringLiteral("admin") : QStringLiteral("accounts");
}

QVariantMap okResult() {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), true);
    return result;
}

QVariantMap errorResult(const QString& message) {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("error"), message);
    return result;
}

QStringList parseCsvLine(const QString& line) {
    QStringList values;
    QString current;
    bool quoted = false;
    for (int index = 0; index < line.size(); ++index) {
        const QChar c = line.at(index);
        if (c == QLatin1Char('"')) {
            if (quoted && index + 1 < line.size() && line.at(index + 1) == QLatin1Char('"')) {
                current.append(QLatin1Char('"'));
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (c == QLatin1Char(',') && !quoted) {
            values.append(current.trimmed());
            current.clear();
        } else {
            current.append(c);
        }
    }
    values.append(current.trimmed());
    return values;
}

QString findExistingPartyName(const QStringList& parties, const QString& name) {
    const QString trimmed = name.trimmed();
    for (const QString& p : parties) {
        if (p.compare(trimmed, Qt::CaseInsensitive) == 0) {
            return p;
        }
    }
    return QString();
}

// ---- DatabaseTask: thread-safe report/export/backup worker ---------------
//
// Threading safety guarantees:
// - Each task carries immutable params (QVariantMap copied by value)
// - Generation counter allows stale request cancellation
// - QPointer guards against receiver destruction
// - SqliteBusinessServices creates per-call connections (QUuid names)
// - ConnectionPool uses per-thread cache keys (QThread::currentThreadId)
// - Timing instrumentation records each stage for diagnostics
// - No QObject instances are passed across threads

class DatabaseTask : public QRunnable {
public:
    enum TaskType {
        GetLedger,
        GetDayBook,
        GetDashboard,
        GetDailySummary,
        GetOutstanding,
        GetTrialBalance,
        GetProfitAndLoss,
        GetPartyBalances,
        GetAuditLogs,
        GetInventoryReport,
        GetTransactions,
        DoExport,
        DoBackup,
        DoRestore
    };

    DatabaseTask(TaskType type, AppContext& context, const QVariantMap& params,
                 QObject* receiver, int generation, qint64 requestStartNs)
        : type_(type), context_(context), params_(params),
          receiver_(receiver), generation_(generation), requestStartNs_(requestStartNs) {}

    void run() override {
        QElapsedTimer timer;
        timer.start();

        QVariantMap timings;
        timings.insert(QStringLiteral("request_start_ns"), requestStartNs_);
        timings.insert(QStringLiteral("db_start_ns"), timer.nsecsElapsed());

        if (type_ == GetLedger) {
            QVariantMap res;
            try {
                domain::ReportRange range;
                range.start = QDate::fromString(params_.value(QStringLiteral("startIso")).toString(), Qt::ISODate);
                range.end = QDate::fromString(params_.value(QStringLiteral("endIso")).toString(), Qt::ISODate);
                range.days = 0;
                res = jsonObjectToMap(context_.services().reports->ledger(params_.value(QStringLiteral("party")).toString(), range));
            } catch (const std::exception& err) {
                res.insert(QStringLiteral("ok"), false);
                res.insert(QStringLiteral("error"), QString::fromUtf8(err.what()));
            }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onLedgerTaskFinished", res, timings);
        } else if (type_ == GetDayBook) {
            QVariantList res;
            try {
                const QDate date = QDate::fromString(params_.value(QStringLiteral("dateIso")).toString(), Qt::ISODate);
                res = jsonArrayToList(context_.services().reports->dayBook(date));
            } catch (...) {}
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onDayBookTaskFinished", res, timings);
        } else if (type_ == GetDashboard) {
            QVariantMap res;
            try {
                res = jsonObjectToMap(context_.services().reports->dashboardMetrics());
            } catch (const std::exception& err) {
                res.insert(QStringLiteral("ok"), false);
                res.insert(QStringLiteral("error"), QString::fromUtf8(err.what()));
            }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onDashboardTaskFinished", res, timings);
        } else if (type_ == GetDailySummary) {
            QVariantList res;
            try {
                const QDate start = QDate::fromString(params_.value(QStringLiteral("startIso")).toString(), Qt::ISODate);
                const QDate end = QDate::fromString(params_.value(QStringLiteral("endIso")).toString(), Qt::ISODate);
                domain::ReportRange range{start, end, 0};
                res = jsonArrayToList(context_.services().reports->dailySummary(range));
            } catch (...) {}
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onDailySummaryTaskFinished", res, timings);
        } else if (type_ == GetOutstanding) {
            QVariantMap res;
            try { res = jsonObjectToMap(context_.services().reports->outstanding()); }
            catch (const std::exception& err) { res = errorResult(QString::fromUtf8(err.what())); }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onOutstandingTaskFinished", res, timings);
        } else if (type_ == GetTrialBalance) {
            QVariantList res;
            try { res = jsonArrayToList(context_.services().reports->trialBalance()); } catch (...) {}
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onTrialBalanceTaskFinished", res, timings);
        } else if (type_ == GetProfitAndLoss) {
            QVariantMap res;
            try { res = jsonObjectToMap(context_.services().reports->profitAndLoss()); }
            catch (const std::exception& err) { res = errorResult(QString::fromUtf8(err.what())); }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onProfitAndLossTaskFinished", res, timings);
        } else if (type_ == GetPartyBalances) {
            QVariantList res;
            try {
                timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
                timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
                res = partyBalancesFromOutstanding(context_.services().reports->outstanding());
            } catch (...) {}
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onPartyBalancesTaskFinished", res, timings);
        } else if (type_ == GetAuditLogs) {
            QVariantList res;
            try { res = jsonArrayToList(context_.services().audit->listAuditLogs(QStringLiteral("native"))); } catch (...) {}
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onAuditLogsTaskFinished", res, timings);
        } else if (type_ == GetInventoryReport) {
            QVariantMap res;
            try {
                const QString financialYear = params_.value(QStringLiteral("financialYear")).toString();
                const int month = params_.value(QStringLiteral("month")).toInt();
                res.insert(QStringLiteral("rows"), jsonArrayToList(context_.services().inventory->loadSnapshot(financialYear, month)));
                res.insert(QStringLiteral("stockRows"), jsonArrayToList(context_.services().inventory->stockValue(financialYear, month)));
            } catch (const std::exception& err) { res = errorResult(QString::fromUtf8(err.what())); }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onInventoryReportTaskFinished", res, timings);
        } else if (type_ == GetTransactions) {
            QVariantList res;
            try {
                const int page = params_.value(QStringLiteral("page")).toInt();
                const int limit = params_.value(QStringLiteral("limit")).toInt();
                const int days = params_.value(QStringLiteral("days")).toInt();
                const QVector<domain::TransactionRow> rows =
                    context_.services().transactions->listTransactions(
                        page > 0 ? page : 1, limit > 0 ? limit : 100, days);
                timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
                timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
                res.reserve(rows.size());
                for (const domain::TransactionRow& row : rows) {
                    QVariantMap item;
                    item.insert(QStringLiteral("id"), row.id);
                    item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
                    item.insert(QStringLiteral("bill_no"), row.billNo);
                    item.insert(QStringLiteral("party"), row.party);
                    item.insert(QStringLiteral("amount"), row.amount);
                    switch (row.type) {
                    case domain::TransactionType::Sale: item.insert(QStringLiteral("type"), QStringLiteral("Sale")); break;
                    case domain::TransactionType::SaleReturn: item.insert(QStringLiteral("type"), QStringLiteral("Sale Return")); break;
                    case domain::TransactionType::Purchase: item.insert(QStringLiteral("type"), QStringLiteral("Purchase")); break;
                    case domain::TransactionType::Expense: item.insert(QStringLiteral("type"), QStringLiteral("Expense")); break;
                    case domain::TransactionType::Receipt: item.insert(QStringLiteral("type"), QStringLiteral("Receipt")); break;
                    }
                    switch (row.mode) {
                    case domain::PaymentMode::Credit: item.insert(QStringLiteral("mode"), QStringLiteral("Credit")); break;
                    case domain::PaymentMode::Cash: item.insert(QStringLiteral("mode"), QStringLiteral("Cash")); break;
                    case domain::PaymentMode::Upi: item.insert(QStringLiteral("mode"), QStringLiteral("UPI")); break;
                    case domain::PaymentMode::Bank: item.insert(QStringLiteral("mode"), QStringLiteral("Bank")); break;
                    }
                    res.append(item);
                }
            } catch (...) {
                timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
                timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            }
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverListResult("onTransactionsTaskFinished", res, timings);
        } else if (type_ == DoExport) {
            QVariantMap res;
            try {
                const QString format = params_.value(QStringLiteral("format")).toString();
                const QString title = params_.value(QStringLiteral("title")).toString();
                const QVariantList columns = params_.value(QStringLiteral("columns")).toList();
                const QVariantList rows = params_.value(QStringLiteral("rows")).toList();

                const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
                const QString baseName = title.simplified().replace(QLatin1Char(' '), QLatin1Char('_'));

                if (format == QStringLiteral("pdf")) {
                    const QString filePath = dir + QStringLiteral("/") + baseName + QStringLiteral(".pdf");
                    // PDF generation in background thread
                    QPdfWriter writer(filePath);
                    writer.setPageSize(QPageSize(QPageSize::A4));
                    writer.setPageOrientation(QPageLayout::Landscape);
                    writer.setResolution(96);
                    writer.setTitle(title);

                    QPainter painter(&writer);
                    if (painter.isActive()) {
                        const int pageW = writer.width();
                        const int margin = 60;
                        int y = margin;

                        QFont titleFont(QStringLiteral("Inter Tight"), 18, QFont::Bold);
                        painter.setFont(titleFont);
                        painter.setPen(QColor(0x1e, 0x24, 0x35));
                        painter.drawText(margin, y + 22, title);
                        y += 50;

                        QFont subFont(QStringLiteral("Inter Tight"), 9);
                        painter.setFont(subFont);
                        painter.setPen(QColor(0x64, 0x74, 0x8b));
                        painter.drawText(margin, y + 12, QStringLiteral("Generated: ") + QDate::currentDate().toString(QStringLiteral("dd MMM yyyy")));
                        y += 30;

                        if (!columns.isEmpty()) {
                            const int colCount = columns.size();
                            const int colW = (pageW - margin * 2) / colCount;
                            const int rowH = 28;

                            QFont headerFont(QStringLiteral("Inter Tight"), 8, QFont::Bold);
                            painter.setFont(headerFont);
                            painter.setPen(Qt::NoPen);
                            painter.setBrush(QColor(0x1b, 0x24, 0x40));
                            painter.drawRect(margin, y, pageW - margin * 2, rowH);
                            painter.setPen(QColor(0xee, 0xf2, 0xfb));
                            for (int c = 0; c < colCount; ++c) {
                                painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH),
                                                 Qt::AlignVCenter | Qt::AlignLeft, columns[c].toString());
                            }
                            y += rowH;

                            QFont dataFont(QStringLiteral("Inter Tight"), 8);
                            painter.setFont(dataFont);
                            for (int r = 0; r < rows.size(); ++r) {
                                if (y + rowH > writer.height() - margin) {
                                    writer.newPage();
                                    y = margin;
                                }
                                const QVariantList row = rows[r].toList();
                                painter.setPen(Qt::NoPen);
                                painter.setBrush(r % 2 == 0 ? QColor(0xf8, 0xfa, 0xfc) : QColor(0xff, 0xff, 0xff));
                                painter.drawRect(margin, y, pageW - margin * 2, rowH);
                                painter.setPen(QColor(0x1e, 0x24, 0x35));
                                for (int c = 0; c < qMin(colCount, row.size()); ++c) {
                                    painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH),
                                                     Qt::AlignVCenter | Qt::AlignLeft, row[c].toString());
                                }
                                painter.setPen(QPen(QColor(0xe5, 0xe7, 0xeb), 1));
                                painter.drawLine(margin, y + rowH, pageW - margin, y + rowH);
                                y += rowH;
                            }
                        }
                        painter.end();
                    }
                    res = okResult();
                    res.insert(QStringLiteral("path"), filePath);
                } else {
                    // CSV/Excel export
                    const QString filePath = dir + QStringLiteral("/") + baseName + QStringLiteral(".csv");
                    QFile file(filePath);
                    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream out(&file);
                        // Header row
                        QStringList headers;
                        headers.reserve(columns.size());
                        for (const QVariant& col : columns) {
                            headers.append(col.toString());
                        }
                        out << headers.join(QLatin1Char(',')) << QStringLiteral("\n");
                        // Data rows
                        for (const QVariant& rowVar : rows) {
                            const QVariantList row = rowVar.toList();
                            QStringList cells;
                            cells.reserve(row.size());
                            for (const QVariant& cell : row) {
                                QString val = cell.toString();
                                if (val.contains(QLatin1Char(',')) || val.contains(QLatin1Char('"'))) {
                                    val = QLatin1Char('"') + val.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
                                }
                                cells.append(val);
                            }
                            out << cells.join(QLatin1Char(',')) << QStringLiteral("\n");
                        }
                        file.close();
                        res = okResult();
                        res.insert(QStringLiteral("path"), filePath);
                    } else {
                        res = errorResult(QStringLiteral("Could not open file for writing"));
                    }
                }
            } catch (const std::exception& err) {
                res = errorResult(QString::fromUtf8(err.what()));
            }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onExportTaskFinished", res, timings);
        } else if (type_ == DoBackup) {
            QVariantMap res;
            try {
                const QString targetDir = params_.value(QStringLiteral("targetDir")).toString().trimmed();
                const domain::BackupResult result = context_.services().backups->backup(targetDir);
                res = okResult();
                res.insert(QStringLiteral("path"), result.path);
                res.insert(QStringLiteral("status"), result.status);
            } catch (const std::exception& err) {
                res = errorResult(QString::fromUtf8(err.what()));
            }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onBackupTaskFinished", res, timings);
        } else if (type_ == DoRestore) {
            QVariantMap res;
            try {
                const QString backupPath = params_.value(QStringLiteral("backupPath")).toString().trimmed();
                context_.services().backups->restore(backupPath);
                res = okResult();
            } catch (const std::exception& err) {
                res = errorResult(QString::fromUtf8(err.what()));
            }
            timings.insert(QStringLiteral("db_end_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_start_ns"), timer.nsecsElapsed());
            timings.insert(QStringLiteral("transform_end_ns"), timer.nsecsElapsed());
            deliverMapResult("onRestoreTaskFinished", res, timings);
        }
    }

private:
    TaskType type_;
    AppContext& context_;
    QVariantMap params_;
    QPointer<QObject> receiver_;
    int generation_;
    qint64 requestStartNs_;

    // Deliver a QVariantMap result safely - checks receiver still exists
    void deliverMapResult(const char* slot, const QVariantMap& result, const QVariantMap& timings) {
        if (receiver_.isNull()) return; // receiver destroyed, discard safely
        QMetaObject::invokeMethod(receiver_.data(), slot, Qt::QueuedConnection,
                                  Q_ARG(int, generation_),
                                  Q_ARG(QVariantMap, result),
                                  Q_ARG(QVariantMap, timings));
    }

    // Deliver a QVariantList result safely - checks receiver still exists
    void deliverListResult(const char* slot, const QVariantList& result, const QVariantMap& timings) {
        if (receiver_.isNull()) return; // receiver destroyed, discard safely
        QMetaObject::invokeMethod(receiver_.data(), slot, Qt::QueuedConnection,
                                  Q_ARG(int, generation_),
                                  Q_ARG(QVariantList, result),
                                  Q_ARG(QVariantMap, timings));
    }
};

} // namespace

QmlBackend::QmlBackend(AppContext& context, QObject* parent)
    : QObject(parent)
    , context_(context) {
    try {
        const QJsonArray rows = context_.services().companies->listCompanies();
        if (!rows.isEmpty()) {
            companyName_ = rows.first().toObject().value(QStringLiteral("name")).toString();
        }
    } catch (...) {
        // Database may not be configured yet; QML will surface config UI.
    }
}

bool QmlBackend::setupRequired() const {
    try {
        return context_.services().auth->setupRequired();
    } catch (...) {
        return false;
    }
}

// ---- Auth & session ------------------------------------------------------

QVariantMap QmlBackend::login(const QString& username, const QString& password) {
    try {
        const domain::Session session = context_.services().auth->login(username, password);
        authenticated_ = true;
        currentUser_ = session.username;
        currentRole_ = roleToText(session.role);
        emit authChanged();
        emit toast(QStringLiteral("Welcome back, %1").arg(session.username), QStringLiteral("success"));
        QVariantMap result = okResult();
        result.insert(QStringLiteral("username"), session.username);
        result.insert(QStringLiteral("role"), currentRole_);
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::setupAdmin(const QString& username, const QString& password) {
    try {
        const QString user = username.trimmed().isEmpty() ? QStringLiteral("admin") : username.trimmed();
        context_.services().auth->setupAdmin(user, password);
        emit setupChanged();
        // Immediately sign in with the freshly created credentials.
        return login(user, password);
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

void QmlBackend::logout() {
    authenticated_ = false;
    currentUser_.clear();
    currentRole_.clear();
    emit authChanged();
}

QVariantList QmlBackend::companies() {
    try {
        return jsonArrayToList(context_.services().companies->listCompanies());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

// ---- Dashboard -----------------------------------------------------------

QVariantMap QmlBackend::dashboard() {
    try {
        return jsonObjectToMap(context_.services().reports->dashboardMetrics());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::salesTrend(int days) {
    // Derive a lightweight daily sales series from recent transactions so the
    // dashboard sparkline reflects live data without a dedicated endpoint.
    try {
        const int span = days > 0 ? days : 30;
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(1, qMin(span * 10, 500), span);

        QMap<QDate, double> byDate;
        const QDate today = QDate::currentDate();
        for (int offset = span - 1; offset >= 0; offset -= 1) {
            byDate.insert(today.addDays(-offset), 0.0);
        }
        for (const domain::TransactionRow& row : rows) {
            if (row.type == domain::TransactionType::Sale && byDate.contains(row.date)) {
                byDate[row.date] += row.amount;
            } else if (row.type == domain::TransactionType::SaleReturn && byDate.contains(row.date)) {
                byDate[row.date] -= row.amount;
            }
        }
        QVariantList series;
        for (auto it = byDate.constBegin(); it != byDate.constEnd(); ++it) {
            QVariantMap point;
            point.insert(QStringLiteral("date"), it.key().toString(Qt::ISODate));
            point.insert(QStringLiteral("value"), it.value());
            series.append(point);
        }
        return series;
    } catch (const std::exception&) {
        return {};
    }
}

// ---- Transactions --------------------------------------------------------

QVariantList QmlBackend::transactions(int page, int limit, int days) {
    try {
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(page > 0 ? page : 1,
                                                                limit > 0 ? limit : 100,
                                                                days);
        QVariantList list;
        list.reserve(rows.size());
        for (const domain::TransactionRow& row : rows) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), row.id);
            item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
            item.insert(QStringLiteral("bill_no"), row.billNo);
            item.insert(QStringLiteral("party"), row.party);
            item.insert(QStringLiteral("amount"), row.amount);
            switch (row.type) {
            case domain::TransactionType::Sale: item.insert(QStringLiteral("type"), QStringLiteral("Sale")); break;
            case domain::TransactionType::SaleReturn: item.insert(QStringLiteral("type"), QStringLiteral("Sale Return")); break;
            case domain::TransactionType::Purchase: item.insert(QStringLiteral("type"), QStringLiteral("Purchase")); break;
            case domain::TransactionType::Expense: item.insert(QStringLiteral("type"), QStringLiteral("Expense")); break;
            case domain::TransactionType::Receipt: item.insert(QStringLiteral("type"), QStringLiteral("Receipt")); break;
            }
            switch (row.mode) {
            case domain::PaymentMode::Credit: item.insert(QStringLiteral("mode"), QStringLiteral("Credit")); break;
            case domain::PaymentMode::Cash: item.insert(QStringLiteral("mode"), QStringLiteral("Cash")); break;
            case domain::PaymentMode::Upi: item.insert(QStringLiteral("mode"), QStringLiteral("UPI")); break;
            case domain::PaymentMode::Bank: item.insert(QStringLiteral("mode"), QStringLiteral("Bank")); break;
            }
            list.append(item);
        }
        return list;
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::addTransaction(const QString& dateIso,
                                       const QString& billNo,
                                       const QString& party,
                                       const QString& type,
                                       const QString& mode,
                                       double amount) {
    auto fail = [this](const QString& message) {
        emit toast(message, QStringLiteral("error"));
        return errorResult(message);
    };
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        if (!date.isValid()) {
            return fail(QStringLiteral("Please choose a valid date"));
        }
        // If no party specified, default to "customer" for quick cash sales
        const QString resolvedParty = party.trimmed().isEmpty()
            ? QStringLiteral("customer")
            : party.trimmed();
        // Credit mode is not allowed for the generic "customer" party
        if (resolvedParty == QStringLiteral("customer") && mode == QStringLiteral("Credit")) {
            return fail(QStringLiteral("Credit mode is not allowed for generic 'customer'. Select a Credit Customer or choose Cash/UPI/Bank."));
        }
        if (amount <= 0.0) {
            return fail(QStringLiteral("Amount must be greater than zero"));
        }
        const int id = context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
            date,
            billNo.trimmed(),
            resolvedParty,
            transactionTypeFromText(type),
            paymentModeFromText(mode),
            amount
        });
        emit dataChanged();
        emit toast(QStringLiteral("Entry saved"), QStringLiteral("success"));
        QVariantMap result = okResult();
        result.insert(QStringLiteral("id"), id);
        return result;
    } catch (const std::exception& err) {
        return fail(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::editTransaction(int id, const QString& dateIso, const QString& billNo, const QString& party, const QString& type, const QString& mode, double amount) {
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        if (!date.isValid()) {
            return errorResult(QStringLiteral("Please choose a valid date"));
        }
        context_.services().transactions->editTransaction(domain::TransactionEditRequest{
            id,
            domain::TransactionCreateRequest{
                date,
                billNo.trimmed(),
                party.trimmed(),
                transactionTypeFromText(type),
                paymentModeFromText(mode),
                amount
            },
            currentUser_
        });
        emit dataChanged();
        emit toast(QStringLiteral("Transaction updated"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::deleteTransaction(int id) {
    try {
        // Capture the transaction for undo before deleting
        const QVector<domain::TransactionRow> txns =
            context_.services().transactions->listTransactions(1, 5000, 0);
        for (const auto& txn : txns) {
            if (txn.id == id) {
                undoBuffer_.transactionId = id;
                undoBuffer_.request = domain::TransactionCreateRequest{
                    txn.date, txn.billNo, txn.party, txn.type, txn.mode, txn.amount
                };
                break;
            }
        }

        context_.services().transactions->deleteTransaction(domain::TransactionDeleteRequest{
            id, currentUser_
        });
        emit dataChanged();
        emit toast(QStringLiteral("Transaction deleted"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::batchDeleteTransactions(const QVariantList& ids) {
    try {
        int count = 0;
        for (const QVariant& idVar : ids) {
            const int id = idVar.toInt();
            context_.services().transactions->deleteTransaction(domain::TransactionDeleteRequest{
                id, currentUser_
            });
            ++count;
        }
        emit dataChanged();
        emit toast(QStringLiteral("Deleted %1 transactions").arg(count), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::undoDeleteTransaction() {
    try {
        if (undoBuffer_.transactionId < 0) {
            return errorResult(QStringLiteral("Nothing to undo"));
        }
        context_.services().transactions->addTransaction(undoBuffer_.request);
        undoBuffer_.transactionId = -1;
        emit dataChanged();
        emit toast(QStringLiteral("Delete undone"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::downloadImportTemplate() {
    try {
        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/M-Finlogs_Import_Template.csv");
        const QString filePath = QFileDialog::getSaveFileName(QApplication::activeWindow(),
            QStringLiteral("Save Import Template"), defaultPath, QStringLiteral("CSV (*.csv)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return errorResult(QStringLiteral("Could not write file: ") + filePath);
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        stream << QStringLiteral("date,bill no,party,type,mode,amount\n");
        stream << QStringLiteral("2026-04-01,INV-001,Acme Corp,Sale,Credit,15000.00\n");
        stream << QStringLiteral("02-04-2026,INV-002,PQR Traders,Sale Return,Cash,3200.00\n");
        stream << QStringLiteral("03-04-2026,,Office Expenses,Expense,UPI,850.00\n");

        file.close();
        emit toast(QStringLiteral("Import template saved"), QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QString QmlBackend::nextBillNumber(const QString& billNo) {
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
    const QString nextNumber =
        QString::number(numberText.toLongLong() + 1).rightJustified(numberText.size(), QLatin1Char('0'));
    return prefix + nextNumber;
}

// ---- Parties -------------------------------------------------------------

QVariantList QmlBackend::parties() {
    try {
        return jsonArrayToList(context_.services().parties->listParties());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QStringList QmlBackend::partyNames() {
    QStringList names;
    try {
        const QJsonArray rows = context_.services().parties->listParties();
        for (const QJsonValue& value : rows) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
            if (!name.isEmpty()) {
                names.append(name);
            }
        }
        names.removeDuplicates();
        names.sort(Qt::CaseInsensitive);
    } catch (...) {
    }
    return names;
}

QVariantMap QmlBackend::createParty(const QString& name, const QString& type, bool creditAllowed) {
    try {
        if (name.trimmed().isEmpty()) {
            return errorResult(QStringLiteral("Party name is required"));
        }
        context_.services().parties->createParty(name.trimmed(), type, creditAllowed);
        emit dataChanged();
        emit toast(QStringLiteral("Party created"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::renameParty(const QString& oldName, const QString& newName) {
    try {
        context_.services().parties->renameParty(oldName, newName, currentUser_);
        emit dataChanged();
        emit toast(QStringLiteral("Party renamed"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Reports -------------------------------------------------------------

QVariantMap QmlBackend::ledger(const QString& party, const QString& startIso, const QString& endIso) {
    try {
        domain::ReportRange range;
        range.start = QDate::fromString(startIso, Qt::ISODate);
        range.end = QDate::fromString(endIso, Qt::ISODate);
        range.days = 0;
        return jsonObjectToMap(context_.services().reports->ledger(party, range));
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::dayBook(const QString& dateIso) {
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        return jsonArrayToList(context_.services().reports->dayBook(date));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantList QmlBackend::dailySummary(const QString& startIso, const QString& endIso) {
    try {
        domain::ReportRange range;
        range.start = QDate::fromString(startIso, Qt::ISODate);
        range.end = QDate::fromString(endIso, Qt::ISODate);
        range.days = 0;
        return jsonArrayToList(context_.services().reports->dailySummary(range));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::outstanding() {
    try {
        return jsonObjectToMap(context_.services().reports->outstanding());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::trialBalance() {
    try {
        return jsonArrayToList(context_.services().reports->trialBalance());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::profitAndLoss() {
    try {
        return jsonObjectToMap(context_.services().reports->profitAndLoss());
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

void QmlBackend::fetchLedger(const QString& party, const QString& startIso, const QString& endIso) {
    const int gen = nextGeneration(DatabaseTask::GetLedger);
    setLoading(DatabaseTask::GetLedger, true);
    QVariantMap params;
    params.insert(QStringLiteral("party"), party);
    params.insert(QStringLiteral("startIso"), startIso);
    params.insert(QStringLiteral("endIso"), endIso);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetLedger, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchDayBook(const QString& dateIso) {
    const int gen = nextGeneration(DatabaseTask::GetDayBook);
    setLoading(DatabaseTask::GetDayBook, true);
    QVariantMap params;
    params.insert(QStringLiteral("dateIso"), dateIso);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetDayBook, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchDashboard() {
    const int gen = nextGeneration(DatabaseTask::GetDashboard);
    setLoading(DatabaseTask::GetDashboard, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetDashboard, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchDailySummary(const QString& startIso, const QString& endIso) {
    const int gen = nextGeneration(DatabaseTask::GetDailySummary);
    setLoading(DatabaseTask::GetDailySummary, true);
    QVariantMap params;
    params.insert(QStringLiteral("startIso"), startIso);
    params.insert(QStringLiteral("endIso"), endIso);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetDailySummary, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchOutstanding() {
    const int gen = nextGeneration(DatabaseTask::GetOutstanding);
    setLoading(DatabaseTask::GetOutstanding, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetOutstanding, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchTrialBalance() {
    const int gen = nextGeneration(DatabaseTask::GetTrialBalance);
    setLoading(DatabaseTask::GetTrialBalance, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetTrialBalance, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchProfitAndLoss() {
    const int gen = nextGeneration(DatabaseTask::GetProfitAndLoss);
    setLoading(DatabaseTask::GetProfitAndLoss, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetProfitAndLoss, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchPartyBalances() {
    const int gen = nextGeneration(DatabaseTask::GetPartyBalances);
    setLoading(DatabaseTask::GetPartyBalances, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetPartyBalances, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchAuditLogs() {
    const int gen = nextGeneration(DatabaseTask::GetAuditLogs);
    setLoading(DatabaseTask::GetAuditLogs, true);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetAuditLogs, context_, {}, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchInventoryReport(const QString& financialYear, int month) {
    const int gen = nextGeneration(DatabaseTask::GetInventoryReport);
    setLoading(DatabaseTask::GetInventoryReport, true);
    QVariantMap params;
    params.insert(QStringLiteral("financialYear"), financialYear);
    params.insert(QStringLiteral("month"), month);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetInventoryReport, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchTransactions(int page, int limit, int days) {
    const int gen = nextGeneration(DatabaseTask::GetTransactions);
    setLoading(DatabaseTask::GetTransactions, true);
    QVariantMap params;
    params.insert(QStringLiteral("page"), page);
    params.insert(QStringLiteral("limit"), limit);
    params.insert(QStringLiteral("days"), days);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::GetTransactions, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchExport(const QString& format, const QString& title,
                             const QVariantList& columns, const QVariantList& rows) {
    const int gen = nextGeneration(DatabaseTask::DoExport);
    setLoading(DatabaseTask::DoExport, true);
    QVariantMap params;
    params.insert(QStringLiteral("format"), format);
    params.insert(QStringLiteral("title"), title);
    params.insert(QStringLiteral("columns"), QVariant::fromValue(columns));
    params.insert(QStringLiteral("rows"), QVariant::fromValue(rows));
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::DoExport, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchBackup(const QString& targetDir) {
    const int gen = nextGeneration(DatabaseTask::DoBackup);
    setLoading(DatabaseTask::DoBackup, true);
    QVariantMap params;
    params.insert(QStringLiteral("targetDir"), targetDir);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::DoBackup, context_, params, this, gen, t.nsecsElapsed()));
}

void QmlBackend::fetchRestore(const QString& backupPath) {
    const int gen = nextGeneration(DatabaseTask::DoRestore);
    setLoading(DatabaseTask::DoRestore, true);
    QVariantMap params;
    params.insert(QStringLiteral("backupPath"), backupPath);
    QElapsedTimer t; t.start();
    QThreadPool::globalInstance()->start(
        new DatabaseTask(DatabaseTask::DoRestore, context_, params, this, gen, t.nsecsElapsed()));
}

// --- onXTaskFinished slots with generation check and timing emission ------

void QmlBackend::onLedgerTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetLedger, false);
    if (isStale(DatabaseTask::GetLedger, generation)) return;
    emit reportTimingComplete(QStringLiteral("ledger"), timings);
    emit ledgerLoaded(result);
}

void QmlBackend::onDayBookTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetDayBook, false);
    if (isStale(DatabaseTask::GetDayBook, generation)) return;
    emit reportTimingComplete(QStringLiteral("dayBook"), timings);
    emit dayBookLoaded(result);
}

void QmlBackend::onDashboardTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetDashboard, false);
    if (isStale(DatabaseTask::GetDashboard, generation)) return;
    emit reportTimingComplete(QStringLiteral("dashboard"), timings);
    emit dashboardLoaded(result);
}

void QmlBackend::onDailySummaryTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetDailySummary, false);
    if (isStale(DatabaseTask::GetDailySummary, generation)) return;
    emit reportTimingComplete(QStringLiteral("dailySummary"), timings);
    emit dailySummaryLoaded(result);
}

void QmlBackend::onOutstandingTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetOutstanding, false);
    if (isStale(DatabaseTask::GetOutstanding, generation)) return;
    emit reportTimingComplete(QStringLiteral("outstanding"), timings);
    emit outstandingLoaded(result);
}

void QmlBackend::onTrialBalanceTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetTrialBalance, false);
    if (isStale(DatabaseTask::GetTrialBalance, generation)) return;
    emit reportTimingComplete(QStringLiteral("trialBalance"), timings);
    emit trialBalanceLoaded(result);
}

void QmlBackend::onProfitAndLossTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetProfitAndLoss, false);
    if (isStale(DatabaseTask::GetProfitAndLoss, generation)) return;
    emit reportTimingComplete(QStringLiteral("profitAndLoss"), timings);
    emit profitAndLossLoaded(result);
}

void QmlBackend::onPartyBalancesTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetPartyBalances, false);
    if (isStale(DatabaseTask::GetPartyBalances, generation)) return;
    emit reportTimingComplete(QStringLiteral("partyBalances"), timings);
    emit partyBalancesLoaded(result);
}

void QmlBackend::onAuditLogsTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetAuditLogs, false);
    if (isStale(DatabaseTask::GetAuditLogs, generation)) return;
    emit reportTimingComplete(QStringLiteral("auditLogs"), timings);
    emit auditLogsLoaded(result);
}

void QmlBackend::onInventoryReportTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetInventoryReport, false);
    if (isStale(DatabaseTask::GetInventoryReport, generation)) return;
    emit reportTimingComplete(QStringLiteral("inventoryReport"), timings);
    emit inventoryReportLoaded(result);
}

void QmlBackend::onTransactionsTaskFinished(int generation, const QVariantList& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::GetTransactions, false);
    if (isStale(DatabaseTask::GetTransactions, generation)) return;
    emit reportTimingComplete(QStringLiteral("transactions"), timings);
    emit transactionsLoaded(result);
}

void QmlBackend::onExportTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::DoExport, false);
    if (isStale(DatabaseTask::DoExport, generation)) return;
    emit reportTimingComplete(QStringLiteral("export"), timings);
    emit exportComplete(result);
    if (result.value(QStringLiteral("ok")).toBool()) {
        emit toast(QStringLiteral("Export complete: ") + result.value(QStringLiteral("path")).toString(),
                   QStringLiteral("success"));
    }
}

void QmlBackend::onBackupTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::DoBackup, false);
    if (isStale(DatabaseTask::DoBackup, generation)) return;
    emit reportTimingComplete(QStringLiteral("backup"), timings);
    emit backupComplete(result);
    if (result.value(QStringLiteral("ok")).toBool()) {
        emit toast(QStringLiteral("Backup complete"), QStringLiteral("success"));
    }
}

void QmlBackend::onRestoreTaskFinished(int generation, const QVariantMap& result, const QVariantMap& timings) {
    setLoading(DatabaseTask::DoRestore, false);
    if (isStale(DatabaseTask::DoRestore, generation)) return;
    emit reportTimingComplete(QStringLiteral("restore"), timings);
    emit restoreComplete(result);
    if (result.value(QStringLiteral("ok")).toBool()) {
        emit toast(QStringLiteral("Database restored successfully"), QStringLiteral("success"));
        emit dataChanged();
    }
}

QVariantMap QmlBackend::saveCashInHand(const QString& dateIso, double amount) {
    try {
        const QDate date = QDate::fromString(dateIso, Qt::ISODate);
        context_.services().reports->saveCashInHand(date, amount);
        emit dataChanged();
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

double QmlBackend::openingCashSeed() {
    try {
        return context_.services().reports->openingCashSeed();
    } catch (...) {
        return 0.0;
    }
}

QVariantMap QmlBackend::saveOpeningCashSeed(double amount) {
    try {
        context_.services().reports->saveOpeningCashSeed(amount);
        emit toast(QStringLiteral("Opening cash saved"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Inventory -----------------------------------------------------------

QVariantList QmlBackend::financialYears() {
    try {
        return jsonArrayToList(context_.services().financialYears->listFinancialYears());
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantList QmlBackend::inventorySnapshot(const QString& financialYear, int month) {
    try {
        return jsonArrayToList(context_.services().inventory->loadSnapshot(financialYear, month));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::saveInventory(const QString& financialYear, int month, const QVariantList& rows) {
    try {
        QJsonArray jsonRows;
        for (const QVariant& row : rows) {
            const QVariantMap map = row.toMap();
            QJsonObject obj;
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
            jsonRows.append(obj);
        }
        context_.services().inventory->saveSnapshot(financialYear, month, jsonRows);
        emit dataChanged();
        emit toast(QStringLiteral("Inventory saved"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantList QmlBackend::stockValue(const QString& financialYear, int month) {
    try {
        return jsonArrayToList(context_.services().inventory->stockValue(financialYear, month));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::inventoryPdfPreview(const QString& financialYear, int month, bool onlyReorder) {
    try {
        // Load the snapshot and convert to InventoryProductRow vector
        const QJsonArray snapshot = context_.services().inventory->loadSnapshot(financialYear, month);
        QVector<domain::InventoryProductRow> rows;
        for (const QJsonValue& value : snapshot) {
            const QJsonObject obj = value.toObject();
            domain::InventoryProductRow row;
            row.name = obj.value(QStringLiteral("name")).toString();
            row.cost = obj.value(QStringLiteral("cost")).toDouble();
            row.minStock = obj.value(QStringLiteral("min_stock")).toDouble();
            for (int day = 1; day <= 31; ++day) {
                const QString qk = QStringLiteral("qty_%1").arg(day, 2, 10, QLatin1Char('0'));
                const QString pk = QStringLiteral("purchase_%1").arg(day, 2, 10, QLatin1Char('0'));
                row.quantities.append(obj.value(qk).toDouble());
                row.purchaseQuantities.append(obj.value(pk).toDouble());
            }
            rows.append(row);
        }
        if (rows.isEmpty()) {
            return errorResult(QStringLiteral("No inventory data to export"));
        }

        const QString monthName = QDate(2000, month, 1).toString(QStringLiteral("MMMM"));
        domain::InventoryPdfMailRequest request{
            QString(), QString(), QString(), QString(),
            QStringLiteral("smtp.gmail.com"), 587,
            QStringLiteral("Inventory Report"), QString(),
            financialYear, monthName, QStringLiteral("last7"),
            onlyReorder, true, rows
        };

        const QByteArray pdf = context_.services().inventory->buildPdfPreview(request);

        // Write to a temp file and open it
        const QString tempPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("MFinlogs_Inventory_%1.pdf")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return errorResult(QStringLiteral("Could not write preview PDF"));
        }
        tempFile.write(pdf);
        tempFile.close();

        QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
        emit toast(QStringLiteral("Inventory PDF report opened"), QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), tempPath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Users (admin) -------------------------------------------------------

QVariantList QmlBackend::users() {
    try {
        return jsonArrayToList(context_.services().users->listUsers(QStringLiteral("native")));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

QVariantMap QmlBackend::createUser(const QString& username, const QString& password, const QString& role) {
    try {
        domain::UserRole userRole = role == QStringLiteral("admin")
            ? domain::UserRole::Admin
            : domain::UserRole::Accounts;
        context_.services().users->createUser(username.trimmed(), password, userRole, QStringLiteral("native"));
        emit dataChanged();
        emit toast(QStringLiteral("User created"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::changePassword(const QString& username, const QString& newPassword) {
    try {
        context_.services().users->changePassword(username.trimmed(), newPassword, QStringLiteral("native"));
        emit toast(QStringLiteral("Password updated"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::deleteUser(const QString& username) {
    try {
        context_.services().users->deleteUser(username.trimmed(), QStringLiteral("native"));
        emit dataChanged();
        emit toast(QStringLiteral("User deleted"), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Database config -----------------------------------------------------

QVariantMap QmlBackend::readDatabaseConfig() {
    try {
        const domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
        QVariantMap result;
        result.insert(QStringLiteral("server"), cfg.server);
        result.insert(QStringLiteral("database"), cfg.database);
        result.insert(QStringLiteral("username"), cfg.username);
        result.insert(QStringLiteral("driver"), cfg.driver);
        result.insert(QStringLiteral("apiBaseUrl"), cfg.apiBaseUrl);
        result.insert(QStringLiteral("backupDir"), cfg.backupDir);
        result.insert(QStringLiteral("useWindowsAuth"), cfg.useWindowsAuth);
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::testDatabaseConfig(const QVariantMap& config) {
    try {
        domain::DatabaseConfig cfg;
        cfg.server = config.value(QStringLiteral("server")).toString();
        cfg.database = config.value(QStringLiteral("database")).toString();
        cfg.username = config.value(QStringLiteral("username")).toString();
        cfg.password = config.value(QStringLiteral("password")).toString();
        cfg.driver = config.value(QStringLiteral("driver")).toString();
        if (cfg.driver.trimmed().isEmpty()) {
            cfg.driver = QStringLiteral("{ODBC Driver 17 for SQL Server}");
        }
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        // testDatabaseConfig throws on failure with a descriptive message;
        // it returns true only on success.
        context_.services().config->testDatabaseConfig(cfg);
        QVariantMap result = okResult();
        result.insert(QStringLiteral("success"), true);
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::saveDatabaseConfig(const QVariantMap& config) {
    try {
        domain::DatabaseConfig cfg;
        cfg.server = config.value(QStringLiteral("server")).toString();
        cfg.database = config.value(QStringLiteral("database")).toString();
        cfg.username = config.value(QStringLiteral("username")).toString();
        cfg.password = config.value(QStringLiteral("password")).toString();
        cfg.driver = config.value(QStringLiteral("driver")).toString();
        if (cfg.driver.trimmed().isEmpty()) {
            cfg.driver = QStringLiteral("{ODBC Driver 17 for SQL Server}");
        }
        cfg.apiBaseUrl = config.value(QStringLiteral("apiBaseUrl")).toString();
        cfg.backupDir = config.value(QStringLiteral("backupDir")).toString();
        cfg.useWindowsAuth = config.value(QStringLiteral("useWindowsAuth"), true).toBool();
        // Test first, then save (same as the reference commit's behavior)
        context_.services().config->testDatabaseConfig(cfg);
        context_.services().config->writeDatabaseConfig(cfg);
        emit toast(QStringLiteral("Database configuration saved. Restart app to apply."), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Backup & restore ----------------------------------------------------

QVariantMap QmlBackend::backupDatabase(const QString& targetDir) {
    try {
        const domain::BackupResult result = context_.services().backups->backup(targetDir.trimmed());
        emit toast(result.status + QStringLiteral(": ") + result.path, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), result.path);
        res.insert(QStringLiteral("status"), result.status);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::autoBackup() {
    try {
        const domain::BackupResult result = context_.services().backups->autoBackup();
        emit toast(result.status + QStringLiteral(": ") + result.path, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), result.path);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::restoreDatabase(const QString& backupPath) {
    try {
        QString path = backupPath.trimmed();

        // If no path provided, open a native file picker. The filter adapts to
        // the active database mode: .bak for SQL Server, .db for local SQLite.
        if (path.isEmpty()) {
            QString filter;
            QString startDir;
            try {
                const domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
                if (cfg.mode == domain::DatabaseMode::Local) {
                    filter = QStringLiteral("SQLite Database (*.db);;All Files (*)");
                } else {
                    filter = QStringLiteral("SQL Backup (*.bak);;All Files (*)");
                }
                startDir = cfg.backupDir.trimmed();
            } catch (...) {
                filter = QStringLiteral("Backup Files (*.bak *.db);;All Files (*)");
            }
            if (startDir.isEmpty()) {
                startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            }
            path = QFileDialog::getOpenFileName(nullptr,
                QStringLiteral("Choose backup file to restore"), startDir, filter);
            if (path.trimmed().isEmpty()) {
                return okResult(); // user cancelled — not an error
            }
        }

        context_.services().backups->restore(path.trimmed());
        emit toast(QStringLiteral("Backup restored successfully. Restart the app to reload."), QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), path.trimmed());
        return res;
    } catch (const std::exception& err) {
        emit toast(QStringLiteral("Restore failed: ") + QString::fromUtf8(err.what()), QStringLiteral("error"));
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Server controls -----------------------------------------------------

QVariantMap QmlBackend::startNativeServer() {
    const bool launched = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        {QStringLiteral("--server-start")}
    );
    if (launched) {
        emit toast(QStringLiteral("Native server start requested"), QStringLiteral("success"));
        return okResult();
    }
    return errorResult(QStringLiteral("Could not start native server process"));
}

QVariantMap QmlBackend::stopNativeServer() {
    const bool launched = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        {QStringLiteral("--server-stop")}
    );
    if (launched) {
        emit toast(QStringLiteral("Native server stop requested"), QStringLiteral("success"));
        return okResult();
    }
    return errorResult(QStringLiteral("Could not stop native server process"));
}

// ---- Import Transactions -------------------------------------------------

QVariantMap QmlBackend::importTransactions() {
    try {
        QWidget* dialogParent = QApplication::activeWindow();
        const QString path = QFileDialog::getOpenFileName(dialogParent,
            QStringLiteral("Import Transactions"), QString(),
            QStringLiteral("CSV Files (*.csv);;Excel Files (*.xlsx *.xls)"));
        if (path.trimmed().isEmpty()) {
            emit toast(QStringLiteral("Import cancelled"), QStringLiteral("info"));
            return okResult();
        }

        const QStringList existingParties = partyNames();

        struct PreviewRow {
            int line;
            QString date;
            QString billNo;
            QString party;
            QString type;
            QString mode;
            double amount;
            QString error;
        };
        QVector<PreviewRow> previewRows;

        if (path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive) ||
            path.endsWith(QStringLiteral(".xls"), Qt::CaseInsensitive)) {
            const app::XlsxParseResult parsed = app::XlsxReader::read(path);
            if (!parsed.error.isEmpty()) {
                emit toast(parsed.error, QStringLiteral("error"));
                return errorResult(parsed.error);
            }
            for (int i = 0; i < parsed.rows.size(); ++i) {
                const auto& row = parsed.rows.at(i);
                PreviewRow preview;
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
                const QString nt = row.type.trimmed().toLower();
                if (nt != QStringLiteral("sale") && nt != QStringLiteral("sale return") &&
                    nt != QStringLiteral("purchase") && nt != QStringLiteral("expense") &&
                    nt != QStringLiteral("receipt") && nt != QStringLiteral("reciept") &&
                    nt != QStringLiteral("sales return") && nt != QStringLiteral("return")) {
                    errors.append(QStringLiteral("Invalid type: ") + row.type);
                }
                const QString nm = row.mode.trimmed().toLower();
                if (nm != QStringLiteral("credit") && nm != QStringLiteral("cash") &&
                    nm != QStringLiteral("upi") && nm != QStringLiteral("gpay") &&
                    nm != QStringLiteral("bank")) {
                    errors.append(QStringLiteral("Invalid mode: ") + row.mode);
                }
                preview.error = errors.isEmpty() ? QString() : errors.join(QStringLiteral("; "));
                previewRows.append(preview);
            }
        } else {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return errorResult(QStringLiteral("Could not open file: ") + path);
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            if (stream.atEnd()) {
                return errorResult(QStringLiteral("File is empty"));
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
                    return errorResult(QStringLiteral("CSV file is missing required column: ") + key);
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

                PreviewRow preview;
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
        }

        int validCount = 0;
        int invalidCount = 0;
        for (const auto& p : previewRows) {
            if (p.error.isEmpty()) ++validCount;
            else ++invalidCount;
        }

        // Show preview dialog
        QDialog dialog(dialogParent);
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
        const QStringList tableHeaders = {
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

        QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        QObject::connect(importBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() != QDialog::Accepted) {
            emit toast(QStringLiteral("Import cancelled"), QStringLiteral("info"));
            return okResult();
        }

        int imported = 0;
        QStringList createdParties;
        for (const auto& p : previewRows) {
            if (!p.error.isEmpty()) continue;

            QDate date = QDate::fromString(p.date, Qt::ISODate);
            if (!date.isValid()) {
                date = QDate::fromString(p.date, QStringLiteral("dd/MM/yyyy"));
            }
            QString partyName = findExistingPartyName(existingParties, p.party);
            if (partyName.isEmpty()) {
                partyName = p.party.trimmed();
                context_.services().parties->createParty(partyName, QString(), false);
                createdParties.append(partyName);
            }

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

        QString msg = QStringLiteral("Imported %1 of %2 valid transactions (%3 skipped)")
            .arg(imported).arg(validCount).arg(invalidCount);
        if (!createdParties.isEmpty()) {
            msg += QStringLiteral("\nNew parties created: ") + createdParties.join(QStringLiteral(", "));
        }
        emit dataChanged();
        emit toast(msg, QStringLiteral("success"));
        QVariantMap result = okResult();
        result.insert(QStringLiteral("imported"), imported);
        result.insert(QStringLiteral("total"), previewRows.size());
        return result;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Smart Bulk Import (XLS/XLSX → auto-create parties + post all entries) -
//
// Parses the user's daily register format:
//   COL entries   → Receipt transaction (cash/UPI collected from debtors)
//   SL:L with Cr  → Sale transaction with Credit mode (named customer, pay later)
//   SL:L with Cash→ Sale transaction with Cash mode (walk-in, paid now)
//   SL:L with Card→ Sale transaction with UPI mode (walk-in, paid now)
//   Split payments→ Two Sale transactions (cash portion + card portion)
//
// Party names are cleaned (strips "/BLNC..." suffix), normalized, and
// auto-created with creditAllowed=true when they don't exist.

QVariantMap QmlBackend::smartImportExcel() {
    try {
        const QString path = QFileDialog::getOpenFileName(QApplication::activeWindow(),
            QStringLiteral("Smart Import — Daily Register"), QString(),
            QStringLiteral("Excel Files (*.xls *.xlsx);;CSV Files (*.csv)"));
        if (path.trimmed().isEmpty()) {
            return okResult();
        }

        emit toast(QStringLiteral("Smart import started — processing file..."), QStringLiteral("info"));

        // Step 1: convert to CSV if needed
        QString csvPath = path;
        const QString lower = path.toLower();

        if (lower.endsWith(QStringLiteral(".xls")) || lower.endsWith(QStringLiteral(".xlsx"))) {
            csvPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                .filePath(QStringLiteral("mfinlogs_smart_import.csv"));

            // Build PowerShell one-liner: open Excel, save as CSV (type 6), quit
            const QString psScript = QStringLiteral(
                "$e=New-Object -ComObject Excel.Application;"
                "$e.Visible=$false;"
                "$w=$e.Workbooks.Open('%1');"
                "$w.Worksheets(1).SaveAs('%2',6);"
                "$w.Close($false);"
                "$e.Quit();"
                "[System.Runtime.Interopservices.Marshal]::ReleaseComObject($e)|Out-Null"
            ).arg(QString(path).replace(QLatin1Char('\''), QStringLiteral("''")),
                  QString(csvPath).replace(QLatin1Char('\''), QStringLiteral("''")));

            QProcess ps;
            ps.start(QStringLiteral("powershell"), QStringList{
                QStringLiteral("-NoProfile"),
                QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                QStringLiteral("-Command"), psScript
            });
            if (!ps.waitForFinished(60000)) {
                return errorResult(QStringLiteral("Could not convert Excel to CSV (timeout)"));
            }
            if (ps.exitCode() != 0) {
                return errorResult(QStringLiteral("Excel conversion failed: ") + QString::fromUtf8(ps.readAllStandardError()));
            }
        }

        // Step 2: read CSV
        QFile csvFile(csvPath);
        if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return errorResult(QStringLiteral("Could not read converted file"));
        }
        QTextStream csvStream(&csvFile);
        csvStream.setEncoding(QStringConverter::Utf8);

        // Skip header rows (first 2 rows are layout headers)
        csvStream.readLine(); // skip row 1
        csvStream.readLine(); // skip row 2

        struct SmartRow {
            int billNo = 0;
            QString type;
            QDate date;
            QString rawParty;
            double saleCol = 0;
            double credit = 0;
            double cash = 0;
            double card = 0;
            bool valid = false;
        };
        QVector<SmartRow> smartRows;

        // Normalize party name: trim, collapse spaces, strip /BLNC suffix
        auto cleanParty = [](const QString& raw) -> QString {
            QString name = raw.trimmed();
            // Remove "/BLNC ..." or "/blnc..." suffix
            int slash = name.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                name = name.left(slash).trimmed();
            }
            // Collapse multiple spaces
            QString cleaned;
            bool lastSpace = false;
            for (const QChar& ch : name) {
                if (ch.isSpace()) {
                    if (!lastSpace) { cleaned.append(QLatin1Char(' ')); lastSpace = true; }
                } else {
                    cleaned.append(ch);
                    lastSpace = false;
                }
            }
            return cleaned.trimmed();
        };

        // Parse date from CSV (Excel saves as M/d/yyyy or dd-MM-yyyy)
        auto parseDate = [](const QString& text) -> QDate {
            QDate d = QDate::fromString(text.trimmed(), Qt::ISODate);
            if (d.isValid()) return d;
            d = QDate::fromString(text.trimmed(), QStringLiteral("M/d/yyyy"));
            if (d.isValid()) return d;
            d = QDate::fromString(text.trimmed(), QStringLiteral("M/d/yy"));
            if (d.isValid()) return d;
            d = QDate::fromString(text.trimmed(), QStringLiteral("dd-MM-yyyy"));
            if (d.isValid()) return d;
            d = QDate::fromString(text.trimmed(), QStringLiteral("dd/MM/yyyy"));
            return d;
        };

        int lineNum = 2;
        while (!csvStream.atEnd()) {
            lineNum++;
            const QString rawLine = csvStream.readLine().trimmed();
            if (rawLine.isEmpty()) continue;

            // Parse CSV line (simple — quoted fields handled)
            QStringList vals;
            QString cur;
            bool quoted = false;
            for (const QChar& ch : rawLine) {
                if (ch == QLatin1Char('"')) {
                    quoted = !quoted;
                } else if (ch == QLatin1Char(',') && !quoted) {
                    vals.append(cur.trimmed());
                    cur.clear();
                } else {
                    cur.append(ch);
                }
            }
            vals.append(cur.trimmed());
            if (vals.size() < 8) continue;

            const QString typeVal = vals.at(1).trimmed().toUpper();
            if (typeVal != QStringLiteral("COL") && typeVal != QStringLiteral("SL:L")) continue;

            SmartRow row;
            row.billNo = vals.at(0).toInt();
            row.type = typeVal;
            row.date = parseDate(vals.at(2));
            row.rawParty = cleanParty(vals.at(3));
            row.saleCol = vals.at(4).toDouble();
            row.credit = vals.at(5).toDouble();
            row.cash = vals.at(6).toDouble();
            row.card = vals.at(7).toDouble();
            row.valid = row.date.isValid() && row.saleCol > 0;
            if (row.valid) {
                smartRows.append(row);
            }
        }
        csvFile.close();

        if (smartRows.isEmpty()) {
            return errorResult(QStringLiteral("No valid transactions found in file"));
        }

        // Step 3: fetch existing parties once
        QStringList existingParties = partyNames();

        struct ImportStat {
            int receipts = 0;
            int cashSales = 0;
            int upiSales = 0;
            int creditSales = 0;
            int partiesCreated = 0;
            int errors = 0;
            double totalReceipts = 0;
            double totalCashSales = 0;
            double totalUpiSales = 0;
            double totalCreditSales = 0;
        };
        ImportStat stat;

        // Helper: find party (case-insensitive) or create it
        auto findOrCreateParty = [&](const QString& name, bool creditAllowed) -> QString {
            if (name.isEmpty()) return QString();
            for (const QString& p : existingParties) {
                if (p.compare(name, Qt::CaseInsensitive) == 0) {
                    return p;
                }
            }
            // Create new party
            try {
                context_.services().parties->createParty(name, QString(), creditAllowed);
                existingParties.prepend(name); // cache
                stat.partiesCreated++;
                return name;
            } catch (...) {
                return QString();
            }
        };

        // Date -> ISO string
        auto dateToIso = [](const QDate& d) -> QString {
            return d.toString(Qt::ISODate);
        };

        // Step 4: process each row
        for (const SmartRow& row : smartRows) {
            if (row.type == QStringLiteral("COL")) {
                // COL = Receipt (money collected from credit customer)
                QString party = findOrCreateParty(row.rawParty, false);
                if (party.isEmpty() && !row.rawParty.isEmpty()) {
                    party = row.rawParty;
                }
                if (party.isEmpty()) { stat.errors++; continue; }

                QString mode = row.cash > 0 ? QStringLiteral("Cash") : QStringLiteral("UPI");
                double amount = row.cash > 0 ? row.cash : row.card;

                try {
                    context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
                        row.date, QString::number(row.billNo), party,
                        domain::TransactionType::Receipt, paymentModeFromText(mode), amount
                    });
                    stat.receipts++;
                    stat.totalReceipts += amount;
                } catch (...) { stat.errors++; }

            } else if (row.type == QStringLiteral("SL:L")) {
                // SL:L = Sale (cash, UPI, or credit)

                // Credit portion
                if (row.credit > 0) {
                    QString party = findOrCreateParty(row.rawParty, true);
                    if (party.isEmpty()) { stat.errors++; continue; }
                    try {
                        context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
                            row.date, QString::number(row.billNo), party,
                            domain::TransactionType::Sale, domain::PaymentMode::Credit, row.credit
                        });
                        stat.creditSales++;
                        stat.totalCreditSales += row.credit;
                    } catch (...) { stat.errors++; }
                }

                // Cash portion
                if (row.cash > 0) {
                    try {
                        context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
                            row.date, QString::number(row.billNo), QStringLiteral("customer"),
                            domain::TransactionType::Sale, domain::PaymentMode::Cash, row.cash
                        });
                        stat.cashSales++;
                        stat.totalCashSales += row.cash;
                    } catch (...) { stat.errors++; }
                }

                // Card/UPI portion
                if (row.card > 0) {
                    try {
                        context_.services().transactions->addTransaction(domain::TransactionCreateRequest{
                            row.date, QString::number(row.billNo), QStringLiteral("customer"),
                            domain::TransactionType::Sale, domain::PaymentMode::Upi, row.card
                        });
                        stat.upiSales++;
                        stat.totalUpiSales += row.card;
                    } catch (...) { stat.errors++; }
                }
            }
        }

        emit dataChanged();

        // Step 5: summary
        QString summary = QStringLiteral(
            "Smart Import complete!\n"
            "  Receipts (COL)     : %1 entries — ₹%2\n"
            "  Cash Sales (SL:L)  : %3 entries — ₹%4\n"
            "  UPI Sales (SL:L)   : %5 entries — ₹%6\n"
            "  Credit Sales (SL:L): %7 entries — ₹%8\n"
            "  New parties created: %9\n"
            "  Errors             : %10\n"
            "  ────────────────────────────────────\n"
            "  Total posted       : %11 entries — ₹%12"
        ).arg(stat.receipts).arg(QLocale(QLocale::English, QLocale::India).toString(stat.totalReceipts, 'f', 2))
         .arg(stat.cashSales).arg(QLocale(QLocale::English, QLocale::India).toString(stat.totalCashSales, 'f', 2))
         .arg(stat.upiSales).arg(QLocale(QLocale::English, QLocale::India).toString(stat.totalUpiSales, 'f', 2))
         .arg(stat.creditSales).arg(QLocale(QLocale::English, QLocale::India).toString(stat.totalCreditSales, 'f', 2))
         .arg(stat.partiesCreated)
         .arg(stat.errors)
         .arg(stat.receipts + stat.cashSales + stat.upiSales + stat.creditSales)
         .arg(QLocale(QLocale::English, QLocale::India).toString(
             stat.totalReceipts + stat.totalCashSales + stat.totalUpiSales + stat.totalCreditSales, 'f', 2));

        emit toast(summary, QStringLiteral("success"));

        QVariantMap result = okResult();
        result.insert(QStringLiteral("receipts"), stat.receipts);
        result.insert(QStringLiteral("cashSales"), stat.cashSales);
        result.insert(QStringLiteral("upiSales"), stat.upiSales);
        result.insert(QStringLiteral("creditSales"), stat.creditSales);
        result.insert(QStringLiteral("errors"), stat.errors);
        return result;

    } catch (const std::exception& err) {
        emit toast(QStringLiteral("Smart import failed: ") + QString::fromUtf8(err.what()), QStringLiteral("error"));
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Export (PDF/Excel) ---------------------------------------------------

QVariantMap QmlBackend::exportTableToPdf(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    try {
        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/") + title.simplified().replace(QLatin1Char(' '), QLatin1Char('_')) + QStringLiteral(".pdf");
        const QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Export PDF"), defaultPath, QStringLiteral("PDF (*.pdf)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult(); // User cancelled
        }

        QPdfWriter writer(filePath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Landscape);
        writer.setResolution(96);
        writer.setTitle(title);

        QPainter painter(&writer);
        if (!painter.isActive()) {
            return errorResult(QStringLiteral("Could not start PDF painter"));
        }

        const int pageW = writer.width();
        const int margin = 60;
        int y = margin;

        // Title
        QFont titleFont(QStringLiteral("Inter Tight"), 18, QFont::Bold);
        painter.setFont(titleFont);
        painter.setPen(QColor(0x1e, 0x24, 0x35));
        painter.drawText(margin, y + 22, title);
        y += 50;

        // Subtitle (date)
        QFont subFont(QStringLiteral("Inter Tight"), 9);
        painter.setFont(subFont);
        painter.setPen(QColor(0x64, 0x74, 0x8b));
        painter.drawText(margin, y + 12, QStringLiteral("Generated: ") + QDate::currentDate().toString(QStringLiteral("dd MMM yyyy")));
        y += 30;

        // Table
        if (columns.isEmpty()) {
            painter.end();
            emit toast(QStringLiteral("PDF exported: ") + filePath, QStringLiteral("success"));
            QVariantMap res = okResult();
            res.insert(QStringLiteral("path"), filePath);
            return res;
        }

        const int colCount = columns.size();
        const int colW = (pageW - margin * 2) / colCount;
        const int rowH = 28;

        // Header row
        QFont headerFont(QStringLiteral("Inter Tight"), 8, QFont::Bold);
        painter.setFont(headerFont);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x1b, 0x24, 0x40));
        painter.drawRect(margin, y, pageW - margin * 2, rowH);
        painter.setPen(QColor(0xee, 0xf2, 0xfb));
        for (int c = 0; c < colCount; ++c) {
            const QString col = columns[c].toString();
            painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH), Qt::AlignVCenter | Qt::AlignLeft, col);
        }
        y += rowH;

        // Data rows
        QFont dataFont(QStringLiteral("Inter Tight"), 8);
        painter.setFont(dataFont);
        for (int r = 0; r < rows.size(); ++r) {
            if (y + rowH > writer.height() - margin) {
                writer.newPage();
                y = margin;
            }
            const QVariantList row = rows[r].toList();
            painter.setPen(Qt::NoPen);
            painter.setBrush(r % 2 == 0 ? QColor(0xf8, 0xfa, 0xfc) : QColor(0xff, 0xff, 0xff));
            painter.drawRect(margin, y, pageW - margin * 2, rowH);
            painter.setPen(QColor(0x1e, 0x24, 0x35));
            for (int c = 0; c < qMin(colCount, row.size()); ++c) {
                painter.drawText(QRect(margin + c * colW + 6, y, colW - 12, rowH), Qt::AlignVCenter | Qt::AlignLeft, row[c].toString());
            }
            // Bottom border
            painter.setPen(QPen(QColor(0xe5, 0xe7, 0xeb), 1));
            painter.drawLine(margin, y + rowH, pageW - margin, y + rowH);
            y += rowH;
        }

        painter.end();
        emit toast(QStringLiteral("PDF exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::exportTableToExcel(const QString& title, const QVariantList& columns, const QVariantList& rows) {
    // CSV export (universally openable in Excel) — no external lib needed
    try {
        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/") + title.simplified().replace(QLatin1Char(' '), QLatin1Char('_')) + QStringLiteral(".csv");
        const QString filePath = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Export CSV/Excel"), defaultPath, QStringLiteral("CSV (*.csv)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return errorResult(QStringLiteral("Could not write file: ") + filePath);
        }
        QTextStream stream(&file);

        // Header
        QStringList headers;
        for (const QVariant& col : columns) {
            headers.append(QStringLiteral("\"%1\"").arg(col.toString().replace(QLatin1Char('"'), QStringLiteral("\"\""))));
        }
        stream << headers.join(QLatin1Char(',')) << QStringLiteral("\n");

        // Data
        for (const QVariant& rowVar : rows) {
            const QVariantList row = rowVar.toList();
            QStringList cells;
            for (const QVariant& cell : row) {
                cells.append(QStringLiteral("\"%1\"").arg(cell.toString().replace(QLatin1Char('"'), QStringLiteral("\"\""))));
            }
            stream << cells.join(QLatin1Char(',')) << QStringLiteral("\n");
        }

        file.close();
        emit toast(QStringLiteral("CSV exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- 7-day Recent Transactions PDF Report --------------------------------

QVariantMap QmlBackend::exportRecentPdf(int days) {
    try {
        const int span = days > 0 ? days : 7;
        const QVector<domain::TransactionRow> rows =
            context_.services().transactions->listTransactions(1, qMin(span * 100, 2000), span);

        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/M-Finlogs_Last_%1_Days.pdf").arg(span);
        const QString filePath = QFileDialog::getSaveFileName(nullptr,
            QStringLiteral("Export %1-Day Report").arg(span), defaultPath, QStringLiteral("PDF (*.pdf)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QPdfWriter writer(filePath);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Landscape);
        writer.setResolution(96);
        writer.setTitle(QStringLiteral("M-Finlogs — Last %1 Days").arg(span));

        QPainter painter(&writer);
        if (!painter.isActive()) {
            return errorResult(QStringLiteral("Could not start PDF writer"));
        }

        const int pageW = writer.width();
        const int pageH = writer.height();
        const int margin = 32;
        const int tableW = pageW - margin * 2;
        int y = margin;

        // --- Title block ---
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 16, QFont::Bold));
        painter.setPen(QColor(0x1e, 0x24, 0x35));
        painter.drawText(margin, y + 20, QStringLiteral("M-Finlogs — Transaction Report"));
        y += 30;
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 9));
        painter.setPen(QColor(0x64, 0x74, 0x8b));
        const QDate today = QDate::currentDate();
        painter.drawText(margin, y + 12,
            QStringLiteral("Period: %1 to %2 | Generated: %3")
                .arg(today.addDays(-span + 1).toString(QStringLiteral("dd MMM yyyy")),
                     today.toString(QStringLiteral("dd MMM yyyy")),
                     today.toString(QStringLiteral("dd MMM yyyy"))));
        y += 28;

        // --- Column layout ---
        const QStringList headers = {
            QStringLiteral("Date"), QStringLiteral("Bill No"), QStringLiteral("Party"),
            QStringLiteral("Type"), QStringLiteral("Mode"), QStringLiteral("Amount")
        };
        const QVector<double> colWeights = {1.1, 1.0, 2.2, 1.0, 0.9, 1.2};
        double totalWeight = 0;
        for (double w : colWeights) totalWeight += w;
        QVector<int> colWidths;
        for (double w : colWeights) colWidths.append(static_cast<int>(tableW * w / totalWeight));

        const int rowH = 22;
        const int headerH = 26;

        auto drawTableHeader = [&](int& yPos) {
            // Dark header bar
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0x1b, 0x24, 0x40));
            painter.drawRect(margin, yPos, tableW, headerH);
            painter.setPen(QColor(0xee, 0xf2, 0xfb));
            painter.setFont(QFont(QStringLiteral("Inter Tight"), 8, QFont::Bold));
            int x = margin;
            for (int c = 0; c < headers.size(); ++c) {
                const Qt::Alignment align = (c == 5) ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
                painter.drawText(QRect(x + 6, yPos, colWidths[c] - 12, headerH), align, headers[c]);
                x += colWidths[c];
            }
            yPos += headerH;
        };

        drawTableHeader(y);

        // --- Data rows ---
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 8));
        double grandTotal = 0.0;
        for (int r = 0; r < rows.size(); ++r) {
            if (y + rowH > pageH - margin) {
                writer.newPage();
                y = margin;
                drawTableHeader(y);
                painter.setFont(QFont(QStringLiteral("Inter Tight"), 8));
            }

            // Alternating bands
            painter.setPen(Qt::NoPen);
            painter.setBrush(r % 2 == 0 ? QColor(0xf8, 0xfa, 0xfc) : QColor(0xff, 0xff, 0xff));
            painter.drawRect(margin, y, tableW, rowH);

            const domain::TransactionRow& row = rows[r];
            QString typeText;
            switch (row.type) {
            case domain::TransactionType::Sale: typeText = QStringLiteral("Sale"); break;
            case domain::TransactionType::SaleReturn: typeText = QStringLiteral("Sale Return"); break;
            case domain::TransactionType::Purchase: typeText = QStringLiteral("Purchase"); break;
            case domain::TransactionType::Expense: typeText = QStringLiteral("Expense"); break;
            case domain::TransactionType::Receipt: typeText = QStringLiteral("Receipt"); break;
            }
            QString modeText;
            switch (row.mode) {
            case domain::PaymentMode::Credit: modeText = QStringLiteral("Credit"); break;
            case domain::PaymentMode::Cash: modeText = QStringLiteral("Cash"); break;
            case domain::PaymentMode::Upi: modeText = QStringLiteral("UPI"); break;
            case domain::PaymentMode::Bank: modeText = QStringLiteral("Bank"); break;
            }
            const QStringList cells = {
                row.date.toString(QStringLiteral("dd-MM-yyyy")),
                row.billNo,
                row.party,
                typeText,
                modeText,
                formatMoney(row.amount)
            };

            painter.setPen(QColor(0x1e, 0x24, 0x35));
            int x = margin;
            for (int c = 0; c < cells.size(); ++c) {
                const Qt::Alignment align = (c == 5) ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
                painter.drawText(QRect(x + 6, y, colWidths[c] - 12, rowH), align, cells[c]);
                x += colWidths[c];
            }
            // Row border
            painter.setPen(QPen(QColor(0xe5, 0xe7, 0xeb), 1));
            painter.drawLine(margin, y + rowH, margin + tableW, y + rowH);
            grandTotal += row.amount;
            y += rowH;
        }

        // --- Footer total ---
        y += 4;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x1b, 0x24, 0x40));
        painter.drawRect(margin, y, tableW, headerH);
        painter.setPen(QColor(0xee, 0xf2, 0xfb));
        painter.setFont(QFont(QStringLiteral("Inter Tight"), 9, QFont::Bold));
        painter.drawText(QRect(margin + 6, y, tableW / 2, headerH), Qt::AlignLeft | Qt::AlignVCenter,
            QStringLiteral("Total: %1 transactions").arg(rows.size()));
        painter.drawText(QRect(margin + tableW / 2, y, tableW / 2 - 6, headerH), Qt::AlignRight | Qt::AlignVCenter,
            formatMoney(grandTotal));

        painter.end();
        emit toast(QStringLiteral("PDF report exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Recent Transactions Excel Export ------------------------------------

QVariantMap QmlBackend::exportRecentExcel(int days) {
    try {
        const int span = days > 0 ? days : 7;
        const QVector<domain::TransactionRow> txnRows =
            context_.services().transactions->listTransactions(1, qMin(span * 100, 2000), span);

        const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/M-Finlogs_Last_%1_Days.csv").arg(span);
        const QString filePath = QFileDialog::getSaveFileName(QApplication::activeWindow(),
            QStringLiteral("Export %1-Day Report").arg(span), defaultPath, QStringLiteral("CSV (*.csv)"));
        if (filePath.trimmed().isEmpty()) {
            return okResult();
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return errorResult(QStringLiteral("Could not write file: ") + filePath);
        }
        QTextStream stream(&file);

        stream << QStringLiteral("\"Date\",\"Bill No\",\"Party\",\"Type\",\"Mode\",\"Amount\"\n");

        for (const domain::TransactionRow& txn : txnRows) {
            QString typeText;
            switch (txn.type) {
            case domain::TransactionType::Sale: typeText = QStringLiteral("Sale"); break;
            case domain::TransactionType::SaleReturn: typeText = QStringLiteral("Sale Return"); break;
            case domain::TransactionType::Purchase: typeText = QStringLiteral("Purchase"); break;
            case domain::TransactionType::Expense: typeText = QStringLiteral("Expense"); break;
            case domain::TransactionType::Receipt: typeText = QStringLiteral("Receipt"); break;
            }
            QString modeText;
            switch (txn.mode) {
            case domain::PaymentMode::Credit: modeText = QStringLiteral("Credit"); break;
            case domain::PaymentMode::Cash: modeText = QStringLiteral("Cash"); break;
            case domain::PaymentMode::Upi: modeText = QStringLiteral("UPI"); break;
            case domain::PaymentMode::Bank: modeText = QStringLiteral("Bank"); break;
            }

            stream << QStringLiteral("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\"\n")
                .arg(txn.date.toString(QStringLiteral("dd-MM-yyyy")),
                     txn.billNo,
                     txn.party,
                     typeText,
                     modeText,
                     QString::number(txn.amount, 'f', 2));
        }

        file.close();
        emit toast(QStringLiteral("CSV exported: ") + filePath, QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), filePath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- All Party Balances (closed from/to the full date range) --------------

QVariantList QmlBackend::allPartyBalances() {
    try {
        return partyBalancesFromOutstanding(context_.services().reports->outstanding());
    } catch (...) {
        return {};
    }
}

// ---- Credit Follow-up List (Dr balance > 0, sorted descending) -----------

QVariantList QmlBackend::creditFollowupList() {
    try {
        const QVariantList all = allPartyBalances();
        QVariantList debtors;
        for (const QVariant& v : all) {
            const QVariantMap entry = v.toMap();
            const double bal = entry.value(QStringLiteral("balance")).toDouble();
            if (bal > 0) {
                debtors.append(entry);
            }
        }
        // Sort descending by balance
        std::sort(debtors.begin(), debtors.end(), [](const QVariant& a, const QVariant& b) {
            return a.toMap().value(QStringLiteral("balance")).toDouble() >
                   b.toMap().value(QStringLiteral("balance")).toDouble();
        });
        return debtors;
    } catch (...) {
        return {};
    }
}

// ---- Today's Closing Report -----------------------------------------------
//
// Returns a snapshot of the current day's financials:
//   openingCash  – cash seed carried forward
//   cashReceipts – COL cash collected from debtors
//   upiReceipts  – COL UPI collected from debtors
//   cashSales    – SL:L cash sales (walk-in)
//   upiSales     – SL:L UPI sales (walk-in)
//   creditSales  – SL:L credit sales
//   closingCash  – opening + cashReceipts + cashSales
//   upiTotal     – upiReceipts + upiSales
//   recentTxns   – list of today's raw transactions for the overlay

QVariantMap QmlBackend::closingReport() {
    try {
        const QDate today = QDate::currentDate();
        const QString todayIso = today.toString(Qt::ISODate);
        const double opening = openingCashSeed();

        // Fetch today's transactions with a wide net (last 1 day, high limit)
        const QVector<domain::TransactionRow> txns =
            context_.services().transactions->listTransactions(1, 2000, 1);

        double cashReceipts = 0, upiReceipts = 0;
        double cashSales = 0, upiSales = 0, creditSales = 0;
        int receiptCount = 0, cashSaleCount = 0, upiSaleCount = 0, creditSaleCount = 0;
        QVariantList recentTxns;

        for (const domain::TransactionRow& txn : txns) {
            if (txn.date != today) continue;

            // Build display map for the overlay
            QVariantMap display;
            display.insert(QStringLiteral("id"), txn.id);
            display.insert(QStringLiteral("billNo"), txn.billNo);
            display.insert(QStringLiteral("party"), txn.party);

            switch (txn.type) {
            case domain::TransactionType::Sale: display.insert(QStringLiteral("type"), QStringLiteral("Sale")); break;
            case domain::TransactionType::Receipt: display.insert(QStringLiteral("type"), QStringLiteral("Receipt")); break;
            default: display.insert(QStringLiteral("type"), QStringLiteral("Other")); break;
            }
            switch (txn.mode) {
            case domain::PaymentMode::Cash: display.insert(QStringLiteral("mode"), QStringLiteral("Cash")); break;
            case domain::PaymentMode::Upi: display.insert(QStringLiteral("mode"), QStringLiteral("UPI")); break;
            case domain::PaymentMode::Credit: display.insert(QStringLiteral("mode"), QStringLiteral("Credit")); break;
            default: display.insert(QStringLiteral("mode"), QStringLiteral("Other")); break;
            }
            display.insert(QStringLiteral("amount"), txn.amount);
            recentTxns.append(display);

            // Categorise
            if (txn.type == domain::TransactionType::Receipt) {
                if (txn.mode == domain::PaymentMode::Cash) {
                    cashReceipts += txn.amount;
                    receiptCount++;
                } else if (txn.mode == domain::PaymentMode::Upi) {
                    upiReceipts += txn.amount;
                    receiptCount++;
                }
            } else if (txn.type == domain::TransactionType::Sale) {
                if (txn.mode == domain::PaymentMode::Credit) {
                    creditSales += txn.amount;
                    creditSaleCount++;
                } else if (txn.mode == domain::PaymentMode::Cash) {
                    cashSales += txn.amount;
                    cashSaleCount++;
                } else if (txn.mode == domain::PaymentMode::Upi) {
                    upiSales += txn.amount;
                    upiSaleCount++;
                }
            }
        }

        const double closingCash = opening + cashReceipts + cashSales;
        const double upiTotal = upiReceipts + upiSales;

        // Compute total receivables using canonical balance from outstanding report
        double totalReceivables = 0.0;
        try {
            const QJsonObject outstandingData = context_.services().reports->outstanding();
            totalReceivables = outstandingData.value(QStringLiteral("total")).toDouble();
        } catch (...) {}

        QVariantMap result;
        result.insert(QStringLiteral("date"), todayIso);
        result.insert(QStringLiteral("openingCash"), opening);
        result.insert(QStringLiteral("cashReceipts"), cashReceipts);
        result.insert(QStringLiteral("upiReceipts"), upiReceipts);
        result.insert(QStringLiteral("cashSales"), cashSales);
        result.insert(QStringLiteral("upiSales"), upiSales);
        result.insert(QStringLiteral("creditSales"), creditSales);
        result.insert(QStringLiteral("closingCash"), closingCash);
        result.insert(QStringLiteral("upiTotal"), upiTotal);
        result.insert(QStringLiteral("totalCollected"), cashReceipts + upiReceipts + cashSales + upiSales);
        result.insert(QStringLiteral("receivables"), totalReceivables);
        result.insert(QStringLiteral("receiptCount"), receiptCount);
        result.insert(QStringLiteral("cashSaleCount"), cashSaleCount);
        result.insert(QStringLiteral("upiSaleCount"), upiSaleCount);
        result.insert(QStringLiteral("creditSaleCount"), creditSaleCount);
        result.insert(QStringLiteral("recentTxns"), recentTxns);
        result.insert(QStringLiteral("ok"), true);
        return result;
    } catch (const std::exception& err) {
        QVariantMap errRes;
        errRes.insert(QStringLiteral("ok"), false);
        errRes.insert(QStringLiteral("error"), QString::fromUtf8(err.what()));
        return errRes;
    }
}

// ---- Party balance lookup ------------------------------------------------

QVariantMap QmlBackend::partyBalance(const QString& partyName) {
    try {
        if (partyName.trimmed().isEmpty()) {
            return QVariantMap();
        }
        // Get ledger with full date range (no filter = all transactions)
        domain::ReportRange range;
        range.start = QDate(2000, 1, 1);
        range.end = QDate::currentDate();
        range.days = 0;
        const QJsonObject ledger = context_.services().reports->ledger(partyName, range);
        const QJsonArray data = ledger.value(QStringLiteral("data")).toArray();

        double balance = 0.0;
        QString lastTxnDate;
        QString lastTxnType;
        double lastTxnAmount = 0.0;

        if (!data.isEmpty()) {
            const QJsonObject lastRow = data.last().toObject();
            balance = lastRow.value(QStringLiteral("balance")).toDouble();
            lastTxnDate = lastRow.value(QStringLiteral("date")).toString();
            lastTxnType = lastRow.value(QStringLiteral("type")).toString();
            lastTxnAmount = lastRow.value(QStringLiteral("amount")).toDouble();
        }

        QVariantMap result;
        result.insert(QStringLiteral("balance"), balance);
        result.insert(QStringLiteral("balanceLabel"), balance >= 0
            ? formatMoney(balance) + QStringLiteral(" Dr")
            : formatMoney(qAbs(balance)) + QStringLiteral(" Cr"));
        result.insert(QStringLiteral("lastDate"), lastTxnDate);
        result.insert(QStringLiteral("lastType"), lastTxnType);
        result.insert(QStringLiteral("lastAmount"), lastTxnAmount);
        result.insert(QStringLiteral("hasData"), !data.isEmpty());
        return result;
    } catch (...) {
        return QVariantMap();
    }
}

// ---- Mode selection (Server / Local / Migration) -------------------------

QString QmlBackend::currentMode() const {
    try {
        const domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
        return cfg.mode == domain::DatabaseMode::Local ? QStringLiteral("local") : QStringLiteral("server");
    } catch (...) {
        return QStringLiteral("server");
    }
}

QVariantMap QmlBackend::selectMode(const QString& mode) {
    try {
        domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
        if (mode == QStringLiteral("local")) {
            cfg.mode = domain::DatabaseMode::Local;
            if (cfg.sqlitePath.trimmed().isEmpty()) {
                cfg.sqlitePath = QDir(context_.appDataDir()).filePath(QStringLiteral("finlogs.db"));
            }
        } else {
            cfg.mode = domain::DatabaseMode::Server;
        }
        context_.services().config->writeDatabaseConfig(cfg);
        emit toast(QStringLiteral("Mode set to '%1'. Restart the app to apply.").arg(mode), QStringLiteral("success"));
        return okResult();
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

QVariantMap QmlBackend::migrateServerToLocal() {
    try {
        domain::DatabaseConfig cfg = context_.services().config->readDatabaseConfig();
        if (cfg.sqlitePath.trimmed().isEmpty()) {
            cfg.sqlitePath = QDir(context_.appDataDir()).filePath(QStringLiteral("finlogs.db"));
        }

        emit toast(QStringLiteral("Migration started — copying all data from SQL Server..."), QStringLiteral("info"));

        const QString resultPath = data::migrateServerToSqlite(cfg, cfg.sqlitePath);

        // Update config to local mode
        cfg.mode = domain::DatabaseMode::Local;
        context_.services().config->writeDatabaseConfig(cfg);

        emit toast(QStringLiteral("Migration complete! Restart app to use local mode."), QStringLiteral("success"));
        QVariantMap res = okResult();
        res.insert(QStringLiteral("path"), resultPath);
        return res;
    } catch (const std::exception& err) {
        return errorResult(QString::fromUtf8(err.what()));
    }
}

// ---- Audit ---------------------------------------------------------------

QVariantList QmlBackend::auditLogs() {
    try {
        return jsonArrayToList(context_.services().audit->listAuditLogs(QStringLiteral("native")));
    } catch (const std::exception& err) {
        emit toast(QString::fromUtf8(err.what()), QStringLiteral("error"));
        return {};
    }
}

// ---- Formatting helpers --------------------------------------------------

QString QmlBackend::formatMoney(double value) const {
    QLocale india(QLocale::English, QLocale::India);
    return india.toString(value, 'f', 2);
}

QString QmlBackend::formatDate(const QString& iso) const {
    const QDate date = QDate::fromString(iso, Qt::ISODate);
    if (!date.isValid()) {
        return iso;
    }
    return date.toString(QStringLiteral("dd MMM yyyy"));
}

QString QmlBackend::todayIso() const {
    return QDate::currentDate().toString(Qt::ISODate);
}

// ---- Request tracking helpers -------------------------------------------

int QmlBackend::nextGeneration(int taskType) {
    // Increment and return the new generation value.
    // This makes any in-flight request for this task type stale.
    return requestTrackers_[taskType].generation.fetchAndAddRelaxed(1) + 1;
}

bool QmlBackend::isStale(int taskType, int generation) const {
    // If the generation delivered does not match current, the result is stale.
    auto it = requestTrackers_.constFind(taskType);
    if (it == requestTrackers_.constEnd()) return true;
    return it->generation.loadRelaxed() != generation;
}

void QmlBackend::setLoading(int taskType, bool loading) {
    if (loadingFlags_.value(taskType, false) != loading) {
        loadingFlags_[taskType] = loading;
        emit loadingStatesChanged();
    }
}

QVariantMap QmlBackend::loadingStates() const {
    QVariantMap states;
    states.insert(QStringLiteral("ledger"), loadingFlags_.value(DatabaseTask::GetLedger, false));
    states.insert(QStringLiteral("dayBook"), loadingFlags_.value(DatabaseTask::GetDayBook, false));
    states.insert(QStringLiteral("dashboard"), loadingFlags_.value(DatabaseTask::GetDashboard, false));
    states.insert(QStringLiteral("dailySummary"), loadingFlags_.value(DatabaseTask::GetDailySummary, false));
    states.insert(QStringLiteral("outstanding"), loadingFlags_.value(DatabaseTask::GetOutstanding, false));
    states.insert(QStringLiteral("trialBalance"), loadingFlags_.value(DatabaseTask::GetTrialBalance, false));
    states.insert(QStringLiteral("profitAndLoss"), loadingFlags_.value(DatabaseTask::GetProfitAndLoss, false));
    states.insert(QStringLiteral("partyBalances"), loadingFlags_.value(DatabaseTask::GetPartyBalances, false));
    states.insert(QStringLiteral("auditLogs"), loadingFlags_.value(DatabaseTask::GetAuditLogs, false));
    states.insert(QStringLiteral("inventoryReport"), loadingFlags_.value(DatabaseTask::GetInventoryReport, false));
    states.insert(QStringLiteral("transactions"), loadingFlags_.value(DatabaseTask::GetTransactions, false));
    states.insert(QStringLiteral("export"), loadingFlags_.value(DatabaseTask::DoExport, false));
    states.insert(QStringLiteral("backup"), loadingFlags_.value(DatabaseTask::DoBackup, false));
    states.insert(QStringLiteral("restore"), loadingFlags_.value(DatabaseTask::DoRestore, false));
    return states;
}

} // namespace mfinlogs::app
