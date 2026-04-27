#pragma once

#include "app/AppContext.h"

#include <QCoreApplication>
#include <QHttpServer>

namespace mfinlogs::app {

class ServerApplication final {
public:
    explicit ServerApplication(AppContext& context);

    int run(quint16 port);

private:
    void registerRoutes();

    AppContext& context_;
    QHttpServer server_;
};

int runServerApplication(int argc, char** argv);
int installServerMode();
int uninstallServerMode();
int startServerMode();
int stopServerMode();

} // namespace mfinlogs::app

