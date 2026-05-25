#include "domain/ServiceRegistry.h"

#include "data/ConfigService.h"
#include "data/SqlBusinessServices.h"

#include <memory>

namespace mfinlogs::domain {

ServiceRegistry createServiceRegistry(const QString& appDataDir) {
    const std::shared_ptr<ConfigService> config = std::make_shared<data::JsonConfigService>(appDataDir);
    return data::createSqlBusinessServices(config);
}

} // namespace mfinlogs::domain
