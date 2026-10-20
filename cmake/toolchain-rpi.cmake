# ==============================================================================
# CMake Toolchain File for Raspberry Pi & ARM Single-Board Computers (SBCs)
# ==============================================================================
# Supports:
#   - 64-bit ARM (aarch64 / arm64) -> Raspberry Pi 3/4/5, Zero 2 W,
#                                    Orange Pi, Rock Pi / Radxa, Odroid, Jetson
#   - 32-bit ARM (armhf / armv7l)  -> Raspberry Pi 2/3/4 (32-bit OS),
#                                    older ARMv7 boards
# ==============================================================================

set(CMAKE_SYSTEM_NAME Linux)

# 1. Determine Target Architecture
if(NOT DEFINED PI_ARCH)
    if(DEFINED CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "")
        set(PI_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    else()
        set(PI_ARCH "aarch64")
    endif()
endif()

if(PI_ARCH STREQUAL "arm64" OR PI_ARCH STREQUAL "aarch64")
    set(PI_ARCH "aarch64")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
    set(CROSS_TRIPLE "aarch64-linux-gnu")
    set(DEB_ARCH "arm64")
elseif(PI_ARCH STREQUAL "armhf" OR PI_ARCH STREQUAL "armv7l" OR PI_ARCH STREQUAL "arm")
    set(PI_ARCH "armhf")
    set(CMAKE_SYSTEM_PROCESSOR "arm")
    set(CROSS_TRIPLE "arm-linux-gnueabihf")
    set(DEB_ARCH "armhf")
else()
    set(CMAKE_SYSTEM_PROCESSOR "${PI_ARCH}")
    set(CROSS_TRIPLE "${PI_ARCH}-linux-gnu")
    set(DEB_ARCH "${PI_ARCH}")
endif()

# 2. Cross-Compiler Detection
if(DEFINED ENV{CROSS_COMPILE})
    set(CROSS_PREFIX "$ENV{CROSS_COMPILE}")
else()
    set(CROSS_PREFIX "${CROSS_TRIPLE}-")
endif()

if(NOT CMAKE_C_COMPILER)
    find_program(CMAKE_C_COMPILER NAMES ${CROSS_PREFIX}gcc ${CROSS_TRIPLE}-gcc)
endif()

if(NOT CMAKE_CXX_COMPILER)
    find_program(CMAKE_CXX_COMPILER NAMES ${CROSS_PREFIX}g++ ${CROSS_TRIPLE}-g++)
endif()

# 3. Sysroot Support (if targeting mounted RPi filesystem or cross-sysroot)
if(DEFINED PI_SYSROOT OR DEFINED SYSROOT OR DEFINED ENV{PI_SYSROOT} OR DEFINED ENV{SYSROOT})
    if(DEFINED PI_SYSROOT)
        set(SYSROOT_DIR "${PI_SYSROOT}")
    elseif(DEFINED SYSROOT)
        set(SYSROOT_DIR "${SYSROOT}")
    elseif(DEFINED ENV{PI_SYSROOT})
        set(SYSROOT_DIR "$ENV{PI_SYSROOT}")
    elseif(DEFINED ENV{SYSROOT})
        set(SYSROOT_DIR "$ENV{SYSROOT}")
    endif()

    if(EXISTS "${SYSROOT_DIR}")
        set(CMAKE_SYSROOT "${SYSROOT_DIR}")
        set(CMAKE_FIND_ROOT_PATH "${SYSROOT_DIR}")
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    endif()
endif()

# 4. Multiarch Search Paths (Debian/Ubuntu multiarch packages)
if(NOT CMAKE_SYSROOT)
    # Search target multiarch paths on host
    if(EXISTS "/usr/lib/${CROSS_TRIPLE}")
        list(APPEND CMAKE_PREFIX_PATH "/usr/lib/${CROSS_TRIPLE}")
        list(APPEND CMAKE_LIBRARY_PATH "/usr/lib/${CROSS_TRIPLE}")
    endif()
    if(EXISTS "/usr/include/${CROSS_TRIPLE}")
        list(APPEND CMAKE_INCLUDE_PATH "/usr/include/${CROSS_TRIPLE}")
    endif()
    
    # Strictly isolate pkg-config to cross-target libraries to prevent host x86_64 pollution
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/${CROSS_TRIPLE}/pkgconfig:/usr/share/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "")
    set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)
endif()

# 5. Architecture-Specific Flags
if(PI_ARCH STREQUAL "armhf")
    add_compile_options(-mfloat-abi=hard -mfpu=neon-vfpv4)
endif()
