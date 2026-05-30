#pragma once

namespace mfinlogs::app {

// Launches the premium Qt Quick ("Aurora") desktop UI. Returns the process
// exit code. Reuses the existing AppContext / ServiceRegistry backend.
int runQmlApplication(int argc, char** argv);

} // namespace mfinlogs::app
