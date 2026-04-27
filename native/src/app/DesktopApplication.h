#pragma once

#include "app/AppContext.h"

#include <QMainWindow>

namespace mfinlogs::app {

class DesktopApplication final : public QMainWindow {
public:
    explicit DesktopApplication(AppContext& context);

private:
    void buildNavigation();
    void buildPlaceholderWorkspace();
    void wireActions();

    AppContext& context_;
};

int runDesktopApplication(int argc, char** argv);

} // namespace mfinlogs::app
