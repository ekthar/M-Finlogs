#pragma once

#include "domain/ServiceInterfaces.h"

#include <memory>

namespace mfinlogs::domain {

struct ServiceRegistry final {
    std::shared_ptr<AuthService> auth;
    std::shared_ptr<UserService> users;
    std::shared_ptr<CompanyService> companies;
    std::shared_ptr<FinancialYearService> financialYears;
    std::shared_ptr<TransactionService> transactions;
    std::shared_ptr<PartyService> parties;
    std::shared_ptr<ReportService> reports;
    std::shared_ptr<InventoryService> inventory;
    std::shared_ptr<BackupService> backups;
    std::shared_ptr<ConfigService> config;
    std::shared_ptr<AuditService> audit;
    std::shared_ptr<UpdateService> updates;
};

ServiceRegistry createServiceRegistry(const QString& appDataDir);

} // namespace mfinlogs::domain
