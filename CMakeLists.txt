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

# Platform & Target Options
if(ANDROID)
    option(BUILD_GUI "Build Qt QML GUI Application" ON)
    option(BUILD_SERVER "Build REST API Server executable" OFF)
    option(BUILD_TUI "Build console TUI executable" OFF)
    option(BUILD_TESTS "Build unit tests" OFF)

    # On Android, BUILD_GUI is required (the mobile app)
    set(FORCE_BUILD_GUI ON)

    # Default Android ABI if not specified
    if(NOT DEFINED ANDROID_ABI)
        if(DEFINED CMAKE_ANDROID_ARCH_ABI)
            set(ANDROID_ABI "${CMAKE_ANDROID_ARCH_ABI}" CACHE STRING "Android ABI")
        else()
            set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI")
        endif()
    endif()

    # Provide fallback if host Qt CMake files are invoked without Android macros
    if(NOT COMMAND qt6_android_apply_arch_suffix)
        macro(qt6_android_apply_arch_suffix target)
        endmacro()
    endif()
    if(NOT COMMAND qt_android_apply_arch_suffix)
        macro(qt_android_apply_arch_suffix target)
        endmacro()
    endif()
    if(NOT COMMAND _qt_internal_collect_qml_root_paths)
        macro(_qt_internal_collect_qml_root_paths target)
        endmacro()
    endif()
else()
    option(BUILD_GUI "Build Qt QML GUI Application" ON)
    option(BUILD_SERVER "Build REST API Server executable" ON)
    option(BUILD_TUI "Build console TUI executable" ON)
    option(BUILD_TESTS "Build unit tests" ON)
endif()

# Qt Creator / CMake debugging support
if(BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
endif()
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(DEFINED ENV{QTDIR})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()
if(DEFINED ENV{QT_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_ROOT}")
endif()

# Enable IDE folders grouping (for Qt Creator, Visual Studio, Xcode)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Place all built desktop binaries directly into the root build directory (build/debug or build/release)
# Note: On Android, output directory layout is managed by Qt's androiddeployqt
if(NOT ANDROID)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# ----------------------------------------------------------------------------
# 1. Threads Library Check
# ----------------------------------------------------------------------------
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# 2. Qt 6 Dependency Check (Only required when GUI is enabled)
# ----------------------------------------------------------------------------
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Core Gui Quick QuickControls2 Network QUIET)
    if(NOT Qt6_FOUND)
        if(FORCE_BUILD_GUI)
            find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network)
        else()
            message(STATUS "Qt6 (Quick/Controls2) not found; disabling BUILD_GUI. 'ir_gui' will be omitted.")
            set(BUILD_GUI OFF CACHE BOOL "Build Qt QML GUI Application" FORCE)
        endif()
    else()
        if(COMMAND qt_policy)
            qt_policy(SET QTP0001 NEW)
        endif()
    endif()
endif()

# ----------------------------------------------------------------------------
# 3. PkgConfig & libusb-1.0 Dependency Check
# ----------------------------------------------------------------------------
if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUSB IMPORTED_TARGET libusb-1.0)
    endif()
else()
    # On Android, USB hardware driver is disabled (GUI connects via network REST API)
    set(LIBUSB_FOUND FALSE CACHE BOOL "Disable libusb on Android" FORCE)
endif()

if(NOT LIBUSB_FOUND AND NOT TARGET PkgConfig::LIBUSB)
    if(BUILD_SERVER OR BUILD_TESTS)
        if(CMAKE_CROSSCOMPILING)
            set(INSTALL_PKG "libusb-1.0-0-dev:${CMAKE_SYSTEM_PROCESSOR}")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                set(INSTALL_PKG "libusb-1.0-0-dev:arm64")
            endif()
            set(INSTALL_CMD "sudo dpkg --add-architecture ${DEB_ARCH} && sudo apt update && sudo apt install ${INSTALL_PKG}")
        else()
            set(INSTALL_CMD "sudo apt install libusb-1.0-0-dev")
        endif()
        message(FATAL_ERROR
            "\n======================================================================\n"
            "[BUILD ERROR] Missing dependency: libusb-1.0\n"
            "Please install the required Debian package:\n"
            "  ${INSTALL_CMD}\n"
            "======================================================================\n"
        )
    else()
        message(STATUS "libusb-1.0 not found; USB transceiver hardware driver will be omitted.")
    endif()
endif()

# ----------------------------------------------------------------------------
# 4. JSON Library (nlohmann_json) Dependency Check
# ----------------------------------------------------------------------------
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    message(STATUS "nlohmann_json not found via find_package; fetching via FetchContent...")
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ----------------------------------------------------------------------------
# 5. HTTP Server Library (cpp-httplib) Dependency Check
# ----------------------------------------------------------------------------
if(BUILD_SERVER OR BUILD_TUI OR BUILD_TESTS)
    if(CMAKE_CROSSCOMPILING)
        # Prevent cpp-httplib from searching host paths for OpenSSL, Brotli, or ZLIB
        set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_REQUIRE_BROTLI OFF CACHE BOOL "" FORCE)

        include(FetchContent)
        message(STATUS "Cross-compiling: fetching standalone cpp-httplib via FetchContent...")
        FetchContent_Declare(
            httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.15.3
        )
        FetchContent_MakeAvailable(httplib)
    else()
        find_package(httplib QUIET)
        if(NOT httplib_FOUND)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(HTTPLIB IMPORTED_TARGET cpp-httplib)
            endif()
            if(TARGET PkgConfig::HTTPLIB)
                add_library(httplib::httplib ALIAS PkgConfig::HTTPLIB)
            else()
                include(FetchContent)
                message(STATUS "cpp-httplib not found; fetching via FetchContent...")
                FetchContent_Declare(
                    httplib
                    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
                    GIT_TAG v0.15.3
                )
                FetchContent_MakeAvailable(httplib)
            endif()
        endif()
    endif()
endif()

# Modular subdirectories matching configuration
add_subdirectory(src/core)

if(BUILD_SERVER)
    add_subdirectory(src/api)
endif()

if(BUILD_GUI)
    add_subdirectory(src/ui_qml)
endif()

if(BUILD_TUI)
    add_subdirectory(src/ui_tui)
endif()

if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

# Project documentation files (visible in Qt Creator and other IDEs)
add_custom_target(docs
    SOURCES
        src/core/README.md
        src/api/README.md
        README.md
)
