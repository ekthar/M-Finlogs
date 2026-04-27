#pragma once

#include "domain/ServiceInterfaces.h"

#include <QString>

namespace mfinlogs::data {

class JsonConfigService final : public domain::ConfigService {
public:
    explicit JsonConfigService(const QString& appDataDir);

    domain::DatabaseConfig readDatabaseConfig() const override;
    void writeDatabaseConfig(const domain::DatabaseConfig& config) override;
    bool testDatabaseConfig(const domain::DatabaseConfig& config) override;

private:
    QString configPath() const;

    QString appDataDir_;
};

} // namespace mfinlogs::data
