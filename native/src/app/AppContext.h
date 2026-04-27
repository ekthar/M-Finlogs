#pragma once

#include "domain/ServiceRegistry.h"

#include <QString>

namespace mfinlogs::app {

class AppContext final {
public:
    explicit AppContext(const QString& organizationName, const QString& applicationName);

    QString appDataDir() const;
    domain::ServiceRegistry& services();

private:
    QString appDataDir_;
    domain::ServiceRegistry services_;
};

} // namespace mfinlogs::app

