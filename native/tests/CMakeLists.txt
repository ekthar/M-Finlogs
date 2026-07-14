# ---------------------------------------------------------------------------
# M-Finlogs Native - Test targets (QtTest)
#
# All test executables are registered with CTest. Run with:
#   ctest --test-dir native/build --output-on-failure
# ---------------------------------------------------------------------------

find_package(Qt6 6.5 REQUIRED COMPONENTS Test Sql)

# ---------------------------------------------------------------------------
# test_balance_calculator - Pure in-memory unit tests for BalanceCalculator.h
# No database or I/O required.
# ---------------------------------------------------------------------------
qt_add_executable(test_balance_calculator
    test_balance_calculator.cpp
)
target_include_directories(test_balance_calculator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(test_balance_calculator PRIVATE
    Qt6::Test
)
add_test(NAME BalanceCalculatorTest COMMAND test_balance_calculator)

# ---------------------------------------------------------------------------
# test_sqlite_services - Integration tests using an in-memory SQLite database.
# Exercises schema creation, user auth, transaction CRUD, and all report
# service methods without touching the filesystem.
# ---------------------------------------------------------------------------
qt_add_executable(test_sqlite_services
    test_sqlite_services.cpp
)
target_include_directories(test_sqlite_services PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(test_sqlite_services PRIVATE
    Qt6::Test
    Qt6::Sql
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    argon2
)
# Link the main app source objects for service implementations
target_sources(test_sqlite_services PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqliteBusinessServices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqliteSchemaInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SchemaInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqlDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqlBusinessServices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/ConnectionPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/MigrationService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/PasswordVerifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/ConfigService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/ServiceRegistry.cpp
)
if(TARGET Qt6::Svg)
    target_link_libraries(test_sqlite_services PRIVATE Qt6::Svg)
endif()
add_test(NAME SqliteServicesTest COMMAND test_sqlite_services)

# ---------------------------------------------------------------------------
# test_qml_backend - Unit tests for QmlBackend formatting/utility methods.
# Tests formatMoney, formatDate, todayIso, nextBillNumber without a real
# database connection.
# ---------------------------------------------------------------------------
qt_add_executable(test_qml_backend
    test_qml_backend.cpp
)
target_include_directories(test_qml_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(test_qml_backend PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Charts
    Qt6::PrintSupport
    Qt6::HttpServer
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    argon2
)
target_sources(test_qml_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/QmlBackend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/AppContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/app/XlsxReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqliteBusinessServices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqliteSchemaInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SchemaInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqlBusinessServices.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/SqlDatabase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/ConnectionPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/PasswordVerifier.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/ConfigService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data/MigrationService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/ServiceRegistry.cpp
)
if(TARGET Qt6::Svg)
    target_link_libraries(test_qml_backend PRIVATE Qt6::Svg)
endif()
add_test(NAME QmlBackendTest COMMAND test_qml_backend)
