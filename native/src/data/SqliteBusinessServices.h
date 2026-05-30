#pragma once

#include "domain/ServiceRegistry.h"

#include <memory>
#include <QString>

namespace mfinlogs::data {

domain::ServiceRegistry createSqliteBusinessServices(const QString& sqlitePath, const std::shared_ptr<domain::ConfigService>& configService);

} // namespace mfinlogs::data
