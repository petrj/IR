cmake_minimum_required(VERSION 3.16)
project(IR_Transceiver_App VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Default to Debug build type if none is specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build.")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Ensure debug symbols (-g) are always included unless explicitly building Release
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-g)
endif()

# Enable IDE folders grouping (for Qt Creator, Visual Studio, Xcode)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Dependencies
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

# Embedded REST API Dependencies (cpp-httplib & nlohmann_json)
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    message(STATUS "nlohmann_json not found on system; fetching via FetchContent...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(httplib QUIET)
if(NOT httplib_FOUND)
    pkg_check_modules(HTTPLIB IMPORTED_TARGET cpp-httplib)
    if(TARGET PkgConfig::HTTPLIB)
        add_library(httplib::httplib ALIAS PkgConfig::HTTPLIB)
    else()
        include(FetchContent)
        message(STATUS "cpp-httplib not found on system; fetching via FetchContent...")
        FetchContent_Declare(
            httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.18.7
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(httplib)
    endif()
endif()

# Testing support
include(CTest)

# Modular subdirectories matching real filesystem folders
add_subdirectory(src/core)
add_subdirectory(src/api)
add_subdirectory(src/ui_tui)
add_subdirectory(tests)

# Project documentation files (visible in Qt Creator and other IDEs)
add_custom_target(docs
    SOURCES
        project_readme.md
        src/core/README.md
        src/api/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
)
