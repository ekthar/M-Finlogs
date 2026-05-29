#include "app/ServerApplication.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <exception>

namespace {

QString serverPidPath() {
    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir directory(appDataDir);
    if (!directory.exists()) {
        directory.mkpath(QStringLiteral("."));
    }
    return directory.filePath(QStringLiteral("server.pid"));
}

void writeServerPid() {
    QFile file(serverPidPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QByteArray::number(QCoreApplication::applicationPid()));
    }
}

QByteArray jsonBytes(const QJsonObject& payload) {
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QByteArray jsonBytes(const QJsonArray& payload) {
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QByteArray errorBytes(const std::exception& err) {
    QJsonObject payload;
    payload.insert(QStringLiteral("detail"), QString::fromUtf8(err.what()));
    return jsonBytes(payload);
}

QString transactionTypeName(mfinlogs::domain::TransactionType type) {
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
    return QStringLiteral("Unknown");
}

QString paymentModeName(mfinlogs::domain::PaymentMode mode) {
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
    return QStringLiteral("Unknown");
}

mfinlogs::domain::ReportRange recentReportRange() {
    const QDate end = QDate::currentDate();
    return mfinlogs::domain::ReportRange{end.addDays(-30), end, 30};
}

} // namespace

namespace mfinlogs::app {

ServerApplication::ServerApplication(AppContext& context)
    : context_(context) {
    registerRoutes();
}

void ServerApplication::registerRoutes() {
    server_.route("/", []() {
        return QStringLiteral("M-Finlogs native server");
    });

    server_.route("/health", []() {
        QJsonObject payload;
        payload.insert(QStringLiteral("status"), QStringLiteral("ok"));
        payload.insert(QStringLiteral("runtime"), QStringLiteral("native"));
        return jsonBytes(payload);
    });

    server_.route("/companies", [this]() {
        try {
            return jsonBytes(context_.services().companies->listCompanies());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/setup/status", [this]() {
        try {
            QJsonObject payload;
            const bool needsSetup = context_.services().auth->setupRequired();
            payload.insert(QStringLiteral("needs_setup"), needsSetup);
            payload.insert(QStringLiteral("reason"), needsSetup ? QStringLiteral("Admin password not configured") : QStringLiteral("Setup already completed"));
            return jsonBytes(payload);
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/financial-years", [this]() {
        try {
            QJsonObject payload;
            payload.insert(QStringLiteral("years"), context_.services().financialYears->listFinancialYears());
            payload.insert(QStringLiteral("selected"), context_.services().financialYears->selectedFinancialYear());
            return jsonBytes(payload);
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/parties", [this]() {
        try {
            return jsonBytes(context_.services().parties->listParties());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/transactions", [this]() {
        try {
            const QVector<domain::TransactionRow> rows = context_.services().transactions->listTransactions(1, 50, 30);
            QJsonArray transactions;
            for (const domain::TransactionRow& row : rows) {
                QJsonObject item;
                item.insert(QStringLiteral("id"), row.id);
                item.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
                item.insert(QStringLiteral("bill_no"), row.billNo);
                item.insert(QStringLiteral("party"), row.party);
                item.insert(QStringLiteral("type"), transactionTypeName(row.type));
                item.insert(QStringLiteral("mode"), paymentModeName(row.mode));
                item.insert(QStringLiteral("amount"), row.amount);
                transactions.append(item);
            }

            QJsonObject payload;
            payload.insert(QStringLiteral("transactions"), transactions);
            payload.insert(QStringLiteral("page"), 1);
            payload.insert(QStringLiteral("limit"), 50);
            payload.insert(QStringLiteral("total"), transactions.size());
            payload.insert(QStringLiteral("total_pages"), 1);
            return jsonBytes(payload);
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/dashboard", [this]() {
        try {
            return jsonBytes(context_.services().reports->dashboardMetrics());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/daily-summary", [this]() {
        try {
            return jsonBytes(context_.services().reports->dailySummary(recentReportRange()));
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/short-excess", [this]() {
        try {
            return jsonBytes(context_.services().reports->shortExcess(recentReportRange()));
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/outstanding", [this]() {
        try {
            return jsonBytes(context_.services().reports->outstanding());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/trial-balance", [this]() {
        try {
            return jsonBytes(context_.services().reports->trialBalance());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/report/pnl", [this]() {
        try {
            return jsonBytes(context_.services().reports->profitAndLoss());
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });

    server_.route("/audit", [this]() {
        try {
            return jsonBytes(context_.services().audit->listAuditLogs(QStringLiteral("native")));
        } catch (const std::exception& err) {
            return errorBytes(err);
        }
    });
}

int ServerApplication::run(quint16 port) {
    const quint16 boundPort = server_.listen(QHostAddress::Any, port);
    if (boundPort == 0) {
        qCritical("Could not start native server on port %u", port);
        return 1;
    }
    writeServerPid();
    return QCoreApplication::exec();
}

int runServerApplication(int argc, char** argv) {
    QCoreApplication qtApp(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("M-Finlogs"));
    QCoreApplication::setApplicationName(QStringLiteral("M-Finlogs"));

    try {
        AppContext context(QStringLiteral("M-Finlogs"), QStringLiteral("M-Finlogs"));
        ServerApplication server(context);
        return server.run(8000);
    } catch (const std::exception& err) {
        qCritical("Native server startup failed: %s", err.what());
        return 1;
    }
}

int installServerMode() {
    const QString command = QStringLiteral("schtasks");
    const QString taskRunCommand = QStringLiteral("\"%1\" --server").arg(QCoreApplication::applicationFilePath());
    const QStringList arguments = {
        QStringLiteral("/Create"),
        QStringLiteral("/F"),
        QStringLiteral("/SC"),
        QStringLiteral("ONLOGON"),
        QStringLiteral("/RL"),
        QStringLiteral("LIMITED"),
        QStringLiteral("/TN"),
        QStringLiteral("M-FinlogsServer"),
        QStringLiteral("/TR"),
        taskRunCommand
    };
    return QProcess::execute(command, arguments);
}

int uninstallServerMode() {
    const QString command = QStringLiteral("schtasks");
    const QStringList arguments = {
        QStringLiteral("/Delete"),
        QStringLiteral("/F"),
        QStringLiteral("/TN"),
        QStringLiteral("M-FinlogsServer")
    };
    return QProcess::execute(command, arguments);
}

int startServerMode() {
    return QProcess::startDetached(QCoreApplication::applicationFilePath(), { QStringLiteral("--server") }) ? 0 : 1;
}

int stopServerMode() {
    QFile file(serverPidPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QString pid = QString::fromUtf8(file.readAll()).trimmed();
    if (pid.isEmpty()) {
        QFile::remove(serverPidPath());
        return 0;
    }

    const QString command = QStringLiteral("taskkill");
    const QStringList arguments = {
        QStringLiteral("/F"),
        QStringLiteral("/PID"),
        pid
    };
    const int exitCode = QProcess::execute(command, arguments);
    QFile::remove(serverPidPath());
    return exitCode;
}

} // namespace mfinlogs::app
