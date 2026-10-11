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

# Qt Creator / CMake debugging support
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(DEFINED ENV{QTDIR})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()
if(DEFINED ENV{QT_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_ROOT}")
endif()

# Enable IDE folders grouping (for Qt Creator, Visual Studio, Xcode)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Place all built binaries directly into the root build directory (build/debug or build/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# ----------------------------------------------------------------------------
# 1. Threads Library Check
# ----------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# 2. Qt 6 GUI Dependency Check
# ----------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)

# ----------------------------------------------------------------------------
# 2. PkgConfig & libusb-1.0 Dependency Check
# ----------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB IMPORTED_TARGET libusb-1.0)
if(NOT LIBUSB_FOUND)
    message(FATAL_ERROR
        "\n======================================================================\n"
        "[BUILD ERROR] Missing dependency: libusb-1.0\n"
        "Please install the required Debian package:\n"
        "  sudo apt install libusb-1.0-0-dev\n"
        "======================================================================\n"
    )
endif()

# ----------------------------------------------------------------------------
# 3. JSON Library (nlohmann_json) Dependency Check
# ----------------------------------------------------------------------------
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR
        "\n======================================================================\n"
        "[BUILD ERROR] Missing dependency: nlohmann-json (nlohmann_json)\n"
        "Please install the required Debian package:\n"
        "  sudo apt install nlohmann-json3-dev\n"
        "======================================================================\n"
    )
endif()

# ----------------------------------------------------------------------------
# 4. HTTP Server Library (cpp-httplib) Dependency Check
# ----------------------------------------------------------------------------
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    pkg_check_modules(HTTPLIB IMPORTED_TARGET cpp-httplib)
    if(TARGET PkgConfig::HTTPLIB)
        add_library(httplib::httplib ALIAS PkgConfig::HTTPLIB)
    else()
        message(FATAL_ERROR
            "\n======================================================================\n"
            "[BUILD ERROR] Missing dependency: cpp-httplib (httplib)\n"
            "Please install the required Debian package:\n"
            "  sudo apt install libcpp-httplib-dev\n"
            "======================================================================\n"
        )
    endif()
endif()

# Testing support
include(CTest)

# Modular subdirectories matching real filesystem folders
add_subdirectory(src/core)
add_subdirectory(src/api)
add_subdirectory(src/ui_qml)
add_subdirectory(src/ui_tui)
add_subdirectory(tests)

# Project documentation files (visible in Qt Creator and other IDEs)
add_custom_target(docs
    SOURCES
        src/core/README.md
        src/api/README.md
        README.md
)
