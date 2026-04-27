#include "domain/ServiceRegistry.h"

#include "data/ConfigService.h"
#include "data/SqlDatabase.h"
#include "domain/DomainErrors.h"

#include <memory>

namespace mfinlogs::domain {
namespace {

class NativeAuthService final : public AuthService {
public:
    bool setupRequired() const override { throw NotImplementedError("AuthService::setupRequired"); }
    Session login(const QString& username, const QString& password) override {
        Q_UNUSED(username);
        Q_UNUSED(password);
        throw NotImplementedError("AuthService::login");
    }
    void setupAdmin(const QString& username, const QString& password) override {
        Q_UNUSED(username);
        Q_UNUSED(password);
        throw NotImplementedError("AuthService::setupAdmin");
    }
};

class NativeUserService final : public UserService {
public:
    QJsonArray listUsers(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        throw NotImplementedError("UserService::listUsers");
    }
    void createUser(const QString& username, const QString& password, UserRole role, const QString& adminToken) override {
        Q_UNUSED(username);
        Q_UNUSED(password);
        Q_UNUSED(role);
        Q_UNUSED(adminToken);
        throw NotImplementedError("UserService::createUser");
    }
    void changePassword(const QString& username, const QString& password, const QString& adminToken) override {
        Q_UNUSED(username);
        Q_UNUSED(password);
        Q_UNUSED(adminToken);
        throw NotImplementedError("UserService::changePassword");
    }
    void deleteUser(const QString& username, const QString& adminToken) override {
        Q_UNUSED(username);
        Q_UNUSED(adminToken);
        throw NotImplementedError("UserService::deleteUser");
    }
};

class NativeCompanyService final : public CompanyService {
public:
    QJsonArray listCompanies() override { throw NotImplementedError("CompanyService::listCompanies"); }
    void createCompany(const QString& name) override {
        Q_UNUSED(name);
        throw NotImplementedError("CompanyService::createCompany");
    }
    void selectCompany(const QString& name) override {
        Q_UNUSED(name);
        throw NotImplementedError("CompanyService::selectCompany");
    }
};

class NativeFinancialYearService final : public FinancialYearService {
public:
    QJsonArray listFinancialYears() override { throw NotImplementedError("FinancialYearService::listFinancialYears"); }
    void selectFinancialYear(const QString& financialYear) override {
        Q_UNUSED(financialYear);
        throw NotImplementedError("FinancialYearService::selectFinancialYear");
    }
    QString selectedFinancialYear() const override { throw NotImplementedError("FinancialYearService::selectedFinancialYear"); }
};

class NativeTransactionService final : public TransactionService {
public:
    int addTransaction(const TransactionCreateRequest& request) override {
        Q_UNUSED(request);
        throw NotImplementedError("TransactionService::addTransaction");
    }
    QVector<TransactionRow> listTransactions(int page, int limit, int days) override {
        Q_UNUSED(page);
        Q_UNUSED(limit);
        Q_UNUSED(days);
        throw NotImplementedError("TransactionService::listTransactions");
    }
    TransactionRow getTransaction(int transactionId) override {
        Q_UNUSED(transactionId);
        throw NotImplementedError("TransactionService::getTransaction");
    }
    void editTransaction(const TransactionEditRequest& request) override {
        Q_UNUSED(request);
        throw NotImplementedError("TransactionService::editTransaction");
    }
    void deleteTransaction(const TransactionDeleteRequest& request) override {
        Q_UNUSED(request);
        throw NotImplementedError("TransactionService::deleteTransaction");
    }
};

class NativePartyService final : public PartyService {
public:
    QJsonArray listParties() override { throw NotImplementedError("PartyService::listParties"); }
    void createParty(const QString& name, const QString& type, bool creditAllowed) override {
        Q_UNUSED(name);
        Q_UNUSED(type);
        Q_UNUSED(creditAllowed);
        throw NotImplementedError("PartyService::createParty");
    }
    void renameParty(const QString& oldName, const QString& newName, const QString& adminUser) override {
        Q_UNUSED(oldName);
        Q_UNUSED(newName);
        Q_UNUSED(adminUser);
        throw NotImplementedError("PartyService::renameParty");
    }
};

class NativeReportService final : public ReportService {
public:
    QJsonObject dashboardMetrics() override { throw NotImplementedError("ReportService::dashboardMetrics"); }
    QJsonObject ledger(const QString& party, const ReportRange& range) override {
        Q_UNUSED(party);
        Q_UNUSED(range);
        throw NotImplementedError("ReportService::ledger");
    }
    QJsonArray dayBook(const QDate& date) override {
        Q_UNUSED(date);
        throw NotImplementedError("ReportService::dayBook");
    }
    QJsonArray dailySummary(const ReportRange& range) override {
        Q_UNUSED(range);
        throw NotImplementedError("ReportService::dailySummary");
    }
    QJsonArray shortExcess(const ReportRange& range) override {
        Q_UNUSED(range);
        throw NotImplementedError("ReportService::shortExcess");
    }
    QJsonObject outstanding() override { throw NotImplementedError("ReportService::outstanding"); }
    QJsonArray trialBalance() override { throw NotImplementedError("ReportService::trialBalance"); }
    QJsonObject profitAndLoss() override { throw NotImplementedError("ReportService::profitAndLoss"); }
};

class NativeInventoryService final : public InventoryService {
public:
    QByteArray buildPdfPreview(const InventoryPdfMailRequest& request) override {
        Q_UNUSED(request);
        throw NotImplementedError("InventoryService::buildPdfPreview");
    }
    void sendPdfMail(const InventoryPdfMailRequest& request) override {
        Q_UNUSED(request);
        throw NotImplementedError("InventoryService::sendPdfMail");
    }
};

class NativeBackupService final : public BackupService {
public:
    BackupResult backup(const QString& destinationPath) override {
        Q_UNUSED(destinationPath);
        throw NotImplementedError("BackupService::backup");
    }
    BackupResult autoBackup() override { throw NotImplementedError("BackupService::autoBackup"); }
    void restore(const QString& backupPath) override {
        Q_UNUSED(backupPath);
        throw NotImplementedError("BackupService::restore");
    }
};

class NativeAuditService final : public AuditService {
public:
    QJsonArray listAuditLogs(const QString& adminToken) override {
        Q_UNUSED(adminToken);
        throw NotImplementedError("AuditService::listAuditLogs");
    }
    void writeAuditLog(const QString& username, const QString& action, const QString& details) override {
        Q_UNUSED(username);
        Q_UNUSED(action);
        Q_UNUSED(details);
        throw NotImplementedError("AuditService::writeAuditLog");
    }
};

class NativeUpdateService final : public UpdateService {
public:
    void checkForUpdates() override { throw NotImplementedError("UpdateService::checkForUpdates"); }
    void restartAndInstall() override { throw NotImplementedError("UpdateService::restartAndInstall"); }
};

} // namespace

ServiceRegistry createServiceRegistry(const QString& appDataDir) {
    ServiceRegistry registry;
    registry.config = std::make_shared<data::JsonConfigService>(appDataDir);
    registry.auth = std::make_shared<NativeAuthService>();
    registry.users = std::make_shared<NativeUserService>();
    registry.companies = std::make_shared<NativeCompanyService>();
    registry.financialYears = std::make_shared<NativeFinancialYearService>();
    registry.transactions = std::make_shared<NativeTransactionService>();
    registry.parties = std::make_shared<NativePartyService>();
    registry.reports = std::make_shared<NativeReportService>();
    registry.inventory = std::make_shared<NativeInventoryService>();
    registry.backups = std::make_shared<NativeBackupService>();
    registry.audit = std::make_shared<NativeAuditService>();
    registry.updates = std::make_shared<NativeUpdateService>();
    return registry;
}

} // namespace mfinlogs::domain
