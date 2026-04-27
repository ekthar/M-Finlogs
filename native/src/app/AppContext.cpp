#include "app/AppContext.h"

#include <QDir>
#include <QStandardPaths>

#include <stdexcept>

namespace mfinlogs::app {

AppContext::AppContext(const QString& organizationName, const QString& applicationName) {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    appDataDir_ = baseDir.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.") + applicationName
        : baseDir;
    QDir directory(appDataDir_);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        throw std::runtime_error("Could not create native app data directory");
    }
    Q_UNUSED(organizationName);
    services_ = domain::createServiceRegistry(appDataDir_);
}

QString AppContext::appDataDir() const {
    return appDataDir_;
}

domain::ServiceRegistry& AppContext::services() {
    return services_;
}

} // namespace mfinlogs::app
