#pragma once

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace mfinlogs::domain {

enum class UserRole {
    Admin,
    Accounts
};

enum class TransactionType {
    Sale,
    SaleReturn,
    Purchase,
    Expense,
    Receipt
};

enum class PaymentMode {
    Credit,
    Cash,
    Upi,
    Bank
};

struct Session final {
    QString username;
    UserRole role;
    QString token;
};

struct DatabaseConfig final {
    QString server;
    QString database;
    QString username;
    QString password;
    QString driver;
    QString apiBaseUrl;
    QString backupDir;
    bool useWindowsAuth;
};

struct TransactionRow final {
    int id;
    QDate date;
    QString billNo;
    QString party;
    TransactionType type;
    PaymentMode mode;
    double amount;
};

struct TransactionCreateRequest final {
    QDate date;
    QString billNo;
    QString party;
    TransactionType type;
    PaymentMode mode;
    double amount;
};

struct TransactionEditRequest final {
    int transactionId;
    QString field;
    QString newValue;
    QString adminUser;
};

struct TransactionDeleteRequest final {
    int transactionId;
    QString adminUser;
};

struct ReportRange final {
    QDate start;
    QDate end;
    int days;
};

struct InventoryProductRow final {
    QString name;
    double cost;
    double minStock;
    QVector<double> quantities;
    QVector<double> purchaseQuantities;
};

struct InventoryPdfMailRequest final {
    QString toEmail;
    QString ccEmail;
    QString senderEmail;
    QString senderPassword;
    QString smtpHost;
    int smtpPort;
    QString subject;
    QString notes;
    QString financialYear;
    QString month;
    QString averageMode;
    bool onlyReorder;
    bool includeStockValue;
    QVector<InventoryProductRow> rows;
};

struct BackupResult final {
    QString status;
    QString path;
    QString warning;
};

} // namespace mfinlogs::domain
