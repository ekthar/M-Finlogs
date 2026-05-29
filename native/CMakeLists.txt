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
)

qt_standard_project_setup()

set(MFINLOGS_NATIVE_SOURCES
    src/main.cpp
    src/app/AppContext.h
    src/app/AppContext.cpp
    src/app/ApplicationMode.h
    src/app/DesktopApplication.h
    src/app/DesktopApplication.cpp
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
    src/domain/DomainErrors.h
    src/domain/ServiceInterfaces.h
    src/domain/ServiceRegistry.h
    src/domain/ServiceRegistry.cpp
    src/domain/Types.h
    resources/native_resources.qrc
)

qt_add_executable(mfinlogs-native
    WIN32
    ${MFINLOGS_NATIVE_SOURCES}
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
