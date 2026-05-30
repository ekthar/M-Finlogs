cmake_minimum_required(VERSION 3.25)

project(MFinlogsNative
    VERSION 0.1.0
    DESCRIPTION "Native C++ rewrite of M-Finlogs"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Widgets
    Sql
    Network
    Charts
    PrintSupport
    HttpServer
    Svg
    Qml
    Quick
    QuickControls2
)

qt_standard_project_setup()

# Explicitly enable automatic resource compilation so the icon qrc is built
# into the binary (AUTORCC was not picking it up reliably).
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

set(MFINLOGS_NATIVE_SOURCES
    src/main.cpp
    src/app/AppContext.h
    src/app/AppContext.cpp
    src/app/ApplicationMode.h
    src/app/DesktopApplication.h
    src/app/DesktopApplication.cpp
    src/app/QmlApplication.h
    src/app/QmlApplication.cpp
    src/app/QmlBackend.h
    src/app/QmlBackend.cpp
    src/app/SplashScreen.h
    src/app/SplashScreen.cpp
    src/app/ServerApplication.h
    src/app/ServerApplication.cpp
    src/data/ConfigService.h
    src/data/ConfigService.cpp
    src/data/SchemaInitializer.h
    src/data/SchemaInitializer.cpp
    src/data/SqlBusinessServices.h
    src/data/SqlBusinessServices.cpp
    src/data/SqlDatabase.h
    src/data/SqlDatabase.cpp
    src/data/SqliteSchemaInitializer.h
    src/data/SqliteSchemaInitializer.cpp
    src/data/SqliteBusinessServices.h
    src/data/SqliteBusinessServices.cpp
    src/data/MigrationService.h
    src/data/MigrationService.cpp
    src/domain/DomainErrors.h
    src/domain/ServiceInterfaces.h
    src/domain/ServiceRegistry.h
    src/domain/ServiceRegistry.cpp
    src/domain/Types.h
)

# Windows: embed the application icon into the .exe (shows in taskbar + Explorer)
if(WIN32)
    list(APPEND MFINLOGS_NATIVE_SOURCES resources/app.rc)
endif()

qt_add_executable(mfinlogs-native
    WIN32
    ${MFINLOGS_NATIVE_SOURCES}
)

# --- Qt Quick (QML) presentation layer ------------------------------------
# The premium "Aurora" desktop UI is implemented in QML and compiled into the
# binary as a Qt Quick module. The C++ service backend is reused unchanged.
qt_policy(SET QTP0001 NEW)

# Theme.qml is a global design-token singleton.
set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(mfinlogs-native
    URI MFinlogs
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/AuroraBackground.qml
        qml/GlassPanel.qml
        qml/PrimaryButton.qml
        qml/GhostButton.qml
        qml/FieldInput.qml
        qml/FieldCombo.qml
        qml/DatePickerField.qml
        qml/MetricCard.qml
        qml/NavButton.qml
        qml/SectionHeader.qml
        qml/StatusPill.qml
        qml/ToastHost.qml
        qml/DataTable.qml
        qml/Sparkline.qml
        qml/LoginView.qml
        qml/AppShell.qml
        qml/pages/DashboardPage.qml
        qml/pages/DailyEntryPage.qml
        qml/pages/PartiesPage.qml
        qml/pages/LedgerPage.qml
        qml/pages/DayBookPage.qml
        qml/pages/DailySummaryPage.qml
        qml/pages/OutstandingPage.qml
        qml/pages/TrialBalancePage.qml
        qml/pages/ProfitLossPage.qml
        qml/pages/InventoryPage.qml
        qml/pages/AuditPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/PlaceholderPage.qml
)

# Compile the Qt resource (icons) explicitly into the target
qt_add_resources(mfinlogs-native "native_resources"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/icons/welcome.svg
        resources/icons/entry.svg
        resources/icons/dashboard.svg
        resources/icons/reports.svg
        resources/icons/ledger.svg
        resources/icons/daybook.svg
        resources/icons/summary.svg
        resources/icons/cash.svg
        resources/icons/expenses.svg
        resources/icons/outstanding.svg
        resources/icons/trend.svg
        resources/icons/inventory.svg
        resources/icons/stock.svg
        resources/icons/party.svg
        resources/icons/audit.svg
        resources/icons/settings.svg
        resources/icons/search.svg
        resources/icons/refresh.svg
        resources/icons/save.svg
        resources/icons/clear.svg
        resources/icons/delete.svg
        resources/icons/database.svg
        resources/icons/keyboard.svg
        resources/icons/runtime.svg
        resources/icons/logo.svg
)

target_include_directories(mfinlogs-native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mfinlogs-native PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::HttpServer
    Qt6::Svg
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

if(WIN32)
    target_compile_definitions(mfinlogs-native PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

install(TARGETS mfinlogs-native
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
