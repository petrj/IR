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

# Find PkgConfig & libusb-1.0
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

# IR Core Static Library
add_library(ir_core STATIC
    src/core/README.md
    src/core/RingBuffer.h
    src/core/IrProtocol.h
    src/core/IrProtocol.cpp
    src/core/IrUsbTransceiver.h
    src/core/IrUsbTransceiver.cpp
)

target_include_directories(ir_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ir_core PUBLIC
    PkgConfig::LIBUSB
    Threads::Threads
)

find_package(Threads REQUIRED)

# IR Core Unit Test Executable
add_executable(test_ir_core
    tests/test_core.cpp
    src/ui_tui/SignalStorage.h
    src/ui_tui/SignalStorage.cpp
)

target_include_directories(test_ir_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_tui
)

target_link_libraries(test_ir_core PRIVATE
    ir_core
)

# Testing Console Application (TUI / CLI for reading, saving, and transmitting IR signals)
add_executable(ir_tui_test
    src/ui_tui/main_tui.cpp
    src/ui_tui/TuiController.h
    src/ui_tui/TuiController.cpp
    src/ui_tui/SignalStorage.h
    src/ui_tui/SignalStorage.cpp
)

target_include_directories(ir_tui_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui_tui
)

target_link_libraries(ir_tui_test PRIVATE
    ir_core
)

include(CTest)
add_test(NAME ir_core_unit_tests COMMAND test_ir_core)

# Project documentation files (visible in Qt Creator and other IDEs)
add_custom_target(docs
    SOURCES
        project_readme.md
        src/core/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
)
