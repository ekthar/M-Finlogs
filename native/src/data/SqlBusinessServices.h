#pragma once

#include "domain/ServiceInterfaces.h"

#include <memory>

namespace mfinlogs::data {

domain::ServiceRegistry createSqlBusinessServices(const std::shared_ptr<domain::ConfigService>& configService);

} // namespace mfinlogs::data
