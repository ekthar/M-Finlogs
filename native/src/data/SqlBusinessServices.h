#pragma once

#include "domain/ServiceRegistry.h"

#include <memory>

namespace mfinlogs::data {

domain::ServiceRegistry createSqlBusinessServices(const std::shared_ptr<domain::ConfigService>& configService);

} // namespace mfinlogs::data
