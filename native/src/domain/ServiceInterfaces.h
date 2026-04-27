#pragma once

#include "domain/Types.h"

#include <QByteArray>
#include <QJsonArray>
#include <QString>
#include <QVector>

namespace mfinlogs::domain {

class AuthService {
public:
    virtual ~AuthService() = default;
    virtual bool setupRequired() const = 0;
    virtual Session login(const QString& username, const QString& password) = 0;
    virtual void setupAdmin(const QString& username, const QString& password) = 0;
};

class UserService {
public:
    virtual ~UserService() = default;
    virtual QJsonArray listUsers(const QString& adminToken) = 0;
    virtual void createUser(const QString& username, const QString& password, UserRole role, const QString& adminToken) = 0;
    virtual void changePassword(const QString& username, const QString& password, const QString& adminToken) = 0;
    virtual void deleteUser(const QString& username, const QString& adminToken) = 0;
};

class CompanyService {
public:
    virtual ~CompanyService() = default;
    virtual QJsonArray listCompanies() = 0;
    virtual void createCompany(const QString& name) = 0;
    virtual void selectCompany(const QString& name) = 0;
};

class FinancialYearService {
public:
    virtual ~FinancialYearService() = default;
    virtual QJsonArray listFinancialYears() = 0;
    virtual void selectFinancialYear(const QString& financialYear) = 0;
    virtual QString selectedFinancialYear() const = 0;
};

class TransactionService {
public:
    virtual ~TransactionService() = default;
    virtual int addTransaction(const TransactionCreateRequest& request) = 0;
    virtual QVector<TransactionRow> listTransactions(int page, int limit, int days) = 0;
    virtual TransactionRow getTransaction(int transactionId) = 0;
    virtual void editTransaction(const TransactionEditRequest& request) = 0;
    virtual void deleteTransaction(const TransactionDeleteRequest& request) = 0;
};

class PartyService {
public:
    virtual ~PartyService() = default;
    virtual QJsonArray listParties() = 0;
    virtual void createParty(const QString& name, const QString& type, bool creditAllowed) = 0;
    virtual void renameParty(const QString& oldName, const QString& newName, const QString& adminUser) = 0;
};

class ReportService {
public:
    virtual ~ReportService() = default;
    virtual QJsonObject dashboardMetrics() = 0;
    virtual QJsonObject ledger(const QString& party, const ReportRange& range) = 0;
    virtual QJsonArray dayBook(const QDate& date) = 0;
    virtual QJsonArray dailySummary(const ReportRange& range) = 0;
    virtual QJsonArray shortExcess(const ReportRange& range) = 0;
    virtual QJsonObject outstanding() = 0;
    virtual QJsonArray trialBalance() = 0;
    virtual QJsonObject profitAndLoss() = 0;
};

class InventoryService {
public:
    virtual ~InventoryService() = default;
    virtual QByteArray buildPdfPreview(const InventoryPdfMailRequest& request) = 0;
    virtual void sendPdfMail(const InventoryPdfMailRequest& request) = 0;
};

class BackupService {
public:
    virtual ~BackupService() = default;
    virtual BackupResult backup(const QString& destinationPath) = 0;
    virtual BackupResult autoBackup() = 0;
    virtual void restore(const QString& backupPath) = 0;
};

class ConfigService {
public:
    virtual ~ConfigService() = default;
    virtual DatabaseConfig readDatabaseConfig() const = 0;
    virtual void writeDatabaseConfig(const DatabaseConfig& config) = 0;
    virtual bool testDatabaseConfig(const DatabaseConfig& config) = 0;
};

class AuditService {
public:
    virtual ~AuditService() = default;
    virtual QJsonArray listAuditLogs(const QString& adminToken) = 0;
    virtual void writeAuditLog(const QString& username, const QString& action, const QString& details) = 0;
};

class UpdateService {
public:
    virtual ~UpdateService() = default;
    virtual void checkForUpdates() = 0;
    virtual void restartAndInstall() = 0;
};

} // namespace mfinlogs::domain

