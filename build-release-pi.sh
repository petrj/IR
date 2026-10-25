#!/usr/bin/env bash
set -e

# ==============================================================================
# IR Transceiver Application - Raspberry Pi & ARM SBC Release Build Script
# ==============================================================================
# Targets:
#   - Raspberry Pi 5, 4, 3, Zero 2 W (64-bit ARM / aarch64 / arm64) [DEFAULT]
#   - Raspberry Pi 2, 3, 4 (32-bit ARM / armhf / armv7l)
#   - Compatible ARM SBCs: Orange Pi, Rock Pi / Radxa, Banana Pi, Libre Computer,
#                          Odroid, Pine64, Nvidia Jetson, etc.
#
# Supports both:
#   1. Native compilation (when run directly on Raspberry Pi or ARM container)
#   2. Cross-compilation (when run on x86_64 or another host architecture)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/pi-release"

# ------------------------------------------------------------------------------
# Default Options
# ------------------------------------------------------------------------------
TARGET_ARCH="${PI_ARCH:-aarch64}"
BUILD_MODE="auto"       # auto | native | cross
BUILD_GUI="auto"        # auto | ON | OFF
BUILD_SERVER="ON"
BUILD_TUI="ON"
BUILD_TESTS="ON"
SYSROOT="${PI_SYSROOT:-${SYSROOT:-}}"
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/toolchain-rpi.cmake"
JOBS="$(nproc 2>/dev/null || echo 4)"
DO_PACKAGE="true"
DO_CLEAN="false"

# ------------------------------------------------------------------------------
# Help / Usage Display
# ------------------------------------------------------------------------------
show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS]

Build the IR Transceiver suite for Raspberry Pi and compatible ARM devices.

Options:
  -a, --arch ARCH       Target architecture:
                          - aarch64 / arm64 (default: Pi 5, 4, 3, Zero 2 W, Orange Pi, Rock Pi, etc.)
                          - armhf / armv7l  (Pi 2, 3, 4 with 32-bit OS)
      --native          Force native compilation using host compiler
      --cross           Force cross-compilation mode
      --headless        Build headless without Qt6 GUI (builds ir_api_server, ir_tui, test_all)
      --no-gui          Alias for --headless
      --gui             Explicitly enable Qt6 QML GUI (ir_gui)
      --no-server       Skip building REST API server (ir_api_server)
      --no-tui          Skip building interactive TUI console (ir_tui)
      --no-tests        Skip building unit tests (test_all)
      --sysroot DIR     Path to target sysroot (or set PI_SYSROOT / SYSROOT env)
      --toolchain FILE  Custom CMake toolchain file (default: cmake/toolchain-rpi.cmake)
  -j, --jobs N          Parallel compilation jobs (default: ${JOBS})
      --clean           Wipe build/pi-release directory before building
      --no-package      Skip creating the deployment .tar.gz bundle
  -h, --help            Show this help message and exit

Environment Variables:
  PI_ARCH               Default target architecture (e.g., aarch64, armhf)
  PI_SYSROOT / SYSROOT  Target sysroot directory for cross-compiling
  CROSS_COMPILE         Prefix for cross-compiler binaries (e.g., aarch64-linux-gnu-)
  QT_ROOT / QTDIR       Path to cross-compiled Qt 6 for Raspberry Pi (if building GUI)

Examples:
  ./build-release-pi.sh                         # Standard 64-bit Pi build (auto-detects native/cross)
  ./build-release-pi.sh --arch armhf            # Build for 32-bit Raspberry Pi OS
  ./build-release-pi.sh --headless              # Headless build (no Qt dependencies required)
  ./build-release-pi.sh --sysroot /mnt/rpi-root # Cross-compile using a mounted Raspberry Pi rootfs
  ./build-release-pi.sh --clean -j8             # Clean rebuild with 8 parallel threads
EOF
}

# ------------------------------------------------------------------------------
# Parse Command-Line Arguments
# ------------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -a|--arch)
            TARGET_ARCH="$2"
            shift 2
            ;;
        --native)
            BUILD_MODE="native"
            shift
            ;;
        --cross)
            BUILD_MODE="cross"
            shift
            ;;
        --headless|--no-gui)
            BUILD_GUI="OFF"
            shift
            ;;
        --gui|--with-gui)
            BUILD_GUI="ON"
            shift
            ;;
        --no-server)
            BUILD_SERVER="OFF"
            shift
            ;;
        --no-tui)
            BUILD_TUI="OFF"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        --sysroot)
            SYSROOT="$2"
            shift 2
            ;;
        --toolchain)
            TOOLCHAIN_FILE="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --clean)
            DO_CLEAN="true"
            shift
            ;;
        --no-package)
            DO_PACKAGE="false"
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $1"
            echo "Run '$(basename "$0") --help' for usage."
            exit 1
            ;;
    esac
done

# ------------------------------------------------------------------------------
# Normalize Target Architecture
# ------------------------------------------------------------------------------
case "${TARGET_ARCH}" in
    aarch64|arm64|armv8*|ARM64|AARCH64)
        TARGET_ARCH="aarch64"
        CROSS_TRIPLE="aarch64-linux-gnu"
        DEB_ARCH="arm64"
        PI_DESC="Raspberry Pi 3/4/5 / Zero 2 W & 64-bit ARM SBCs (aarch64)"
        ;;
    armhf|armv7*|arm|ARMHF|ARM)
        TARGET_ARCH="armhf"
        CROSS_TRIPLE="arm-linux-gnueabihf"
        DEB_ARCH="armhf"
        PI_DESC="Raspberry Pi 2/3/4 (32-bit OS) & ARMv7 SBCs (armhf)"
        ;;
    *)
        CROSS_TRIPLE="${TARGET_ARCH}-linux-gnu"
        DEB_ARCH="${TARGET_ARCH}"
        PI_DESC="Custom ARM Target (${TARGET_ARCH})"
        ;;
esac

# ------------------------------------------------------------------------------
# Detect Native vs Cross-Compilation Mode
# ------------------------------------------------------------------------------
HOST_ARCH="$(uname -m)"

if [ "${BUILD_MODE}" = "auto" ]; then
    if [ "${HOST_ARCH}" = "aarch64" ] && [ "${TARGET_ARCH}" = "aarch64" ]; then
        BUILD_MODE="native"
    elif [[ "${HOST_ARCH}" =~ ^armv[67] ]] && [ "${TARGET_ARCH}" = "armhf" ]; then
        BUILD_MODE="native"
    elif [ "${HOST_ARCH}" = "${TARGET_ARCH}" ]; then
        BUILD_MODE="native"
    else
        BUILD_MODE="cross"
    fi
fi

# ------------------------------------------------------------------------------
# Toolchain & Compiler Verification
# ------------------------------------------------------------------------------
CMAKE_CMD="cmake"
CMAKE_EXTRA_ARGS=()

# Verify CMake is installed
if ! command -v "${CMAKE_CMD}" &>/dev/null; then
    echo -e "\n======================================================================"
    echo "[ERROR] CMake build system '${CMAKE_CMD}' not found in PATH!"
    echo "======================================================================"
    echo "To install CMake on Raspberry Pi OS / Debian / Ubuntu, run:"
    echo "  sudo apt update"
    echo "  sudo apt install -y cmake"
    echo ""
    echo "For a complete headless build setup (compiler + libraries), run:"
    echo "  sudo apt update && sudo apt install -y build-essential cmake libusb-1.0-0-dev"
    echo "======================================================================"
    exit 1
fi

if [ "${BUILD_MODE}" = "native" ]; then
    echo "[*] Detected Native Compilation on ${HOST_ARCH} device."
    CC="${CC:-gcc}"
    CXX="${CXX:-g++}"

    if ! command -v "${CXX}" &>/dev/null; then
        echo -e "\n======================================================================"
        echo "[ERROR] Native C++ compiler '${CXX}' not found!"
        echo "======================================================================"
        echo "Please install build tools:"
        echo "  sudo apt update && sudo apt install -y build-essential cmake"
        echo "======================================================================"
        exit 1
    fi
else
    echo "[*] Detected Cross-Compilation (${HOST_ARCH} host -> ${TARGET_ARCH} target)."

    # Determine compiler executables
    if [ -n "${CROSS_COMPILE}" ]; then
        CROSS_CC="${CROSS_COMPILE}gcc"
        CROSS_CXX="${CROSS_COMPILE}g++"
    else
        CROSS_CC="${CROSS_TRIPLE}-gcc"
        CROSS_CXX="${CROSS_TRIPLE}-g++"
    fi

    # Verify cross-compiler availability
    if ! command -v "${CROSS_CXX}" &>/dev/null && [ ! -f "${CROSS_CXX}" ]; then
        echo -e "\n======================================================================"
        echo "[ERROR] Cross-compiler '${CROSS_CXX}' not found in PATH!"
        echo "======================================================================"
        echo "To cross-compile for ${PI_DESC} from your host, please install the cross-toolchain:"
        echo ""
        echo "  On Debian / Ubuntu / Linux Mint:"
        echo "    sudo apt update"
        echo "    sudo apt install gcc-${CROSS_TRIPLE} g++-${CROSS_TRIPLE}"
        echo ""
        echo "  For hardware USB transceiver support (libusb cross-library):"
        echo "    sudo dpkg --add-architecture ${DEB_ARCH}"
        echo "    sudo apt update"
        echo "    sudo apt install libusb-1.0-0-dev:${DEB_ARCH}"
        echo ""
        echo "  On Fedora / RHEL:"
        echo "    sudo dnf install gcc-${CROSS_TRIPLE} gcc-c++-${CROSS_TRIPLE}"
        echo ""
        echo "  On Arch Linux:"
        echo "    sudo pacman -S ${CROSS_TRIPLE}-gcc"
        echo ""
        echo "Alternative Options:"
        echo "  1. If running directly on Raspberry Pi, use:  $(basename "$0") --native"
        echo "  2. If using custom toolchain:               export CROSS_COMPILE=/path/to/prefix-"
        echo "  3. If targeting mounted rootfs:             $(basename "$0") --sysroot /path/to/sysroot"
        echo "======================================================================"
        exit 1
    fi

    # Pass cross toolchain configuration to CMake
    if [ -f "${TOOLCHAIN_FILE}" ]; then
        CMAKE_EXTRA_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}")
    fi
    CMAKE_EXTRA_ARGS+=(
        "-DCMAKE_SYSTEM_NAME=Linux"
        "-DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH}"
        "-DCMAKE_C_COMPILER=${CROSS_CC}"
        "-DCMAKE_CXX_COMPILER=${CROSS_CXX}"
        "-DPI_ARCH=${TARGET_ARCH}"
    )

    if [ -n "${SYSROOT}" ]; then
        if [ ! -d "${SYSROOT}" ]; then
            echo "[ERROR] Specified sysroot directory does not exist: ${SYSROOT}"
            exit 1
        fi
        CMAKE_EXTRA_ARGS+=(
            "-DCMAKE_SYSROOT=${SYSROOT}"
            "-DPI_SYSROOT=${SYSROOT}"
            "-DCMAKE_FIND_ROOT_PATH=${SYSROOT}"
        )
    else
        # Check for target libusb development package if building on Debian/Ubuntu without sysroot
        if [ ! -f "/usr/lib/${CROSS_TRIPLE}/pkgconfig/libusb-1.0.pc" ] && \
           [ ! -f "/usr/lib/${CROSS_TRIPLE}/libusb-1.0.so" ] && \
           [ ! -f "/usr/lib/${CROSS_TRIPLE}/libusb-1.0.a" ]; then
            echo -e "\n======================================================================"
            echo "[ERROR] Target USB library 'libusb-1.0' for ${TARGET_ARCH} (${DEB_ARCH}) not found!"
            echo "======================================================================"
            echo "The IR transceiver driver requires libusb-1.0 for ${TARGET_ARCH}."
            echo "Please install the Debian/Ubuntu multiarch package with:"
            echo ""
            echo "  sudo dpkg --add-architecture ${DEB_ARCH}"
            echo "  sudo apt update"
            echo "  sudo apt install -y libusb-1.0-0-dev:${DEB_ARCH}"
            echo ""
            echo "Once installed, rerun: ./$(basename "$0")"
            echo "======================================================================"
            exit 1
        fi
    fi
fi

# ------------------------------------------------------------------------------
# GUI vs Headless Mode Decision
# ------------------------------------------------------------------------------
if [ "${BUILD_GUI}" = "auto" ]; then
    if [ "${BUILD_MODE}" = "native" ]; then
        # Check if Qt6 is installed natively
        if pkg-config --exists Qt6Core 2>/dev/null || command -v qmake6 &>/dev/null; then
            BUILD_GUI="ON"
        else
            echo "[*] Qt6 not detected natively; building headless (API server + TUI)."
            BUILD_GUI="OFF"
        fi
    else
        # When cross-compiling, default to headless unless Qt cross path is provided
        if [ -n "${QT_ROOT}" ] || [ -n "${QTDIR}" ] || [ -n "${SYSROOT}" ]; then
            BUILD_GUI="ON"
        else
            echo "[*] Cross-compiling without explicit Qt6 SDK; defaulting to HEADLESS mode."
            echo "    (ir_api_server, ir_tui, and test_all will be built without requiring Qt6)"
            BUILD_GUI="OFF"
        fi
    fi
fi

if [ -n "${QT_ROOT}" ]; then
    CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${QT_ROOT}")
elif [ -n "${QTDIR}" ]; then
    CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${QTDIR}")
fi

# ------------------------------------------------------------------------------
# Banner Output
# ------------------------------------------------------------------------------
echo "======================================================================"
echo " Building IR Transceiver Suite for Raspberry Pi & ARM Devices"
echo "======================================================================"
echo " Target Architecture: ${TARGET_ARCH} (${PI_DESC})"
echo " Build Mode:          ${BUILD_MODE^^}"
echo " Source Directory:    ${SCRIPT_DIR}"
echo " Build Directory:     ${BUILD_DIR}"
echo " Parallel Jobs:       ${JOBS}"
echo " REST API Server:     ${BUILD_SERVER}"
echo " TUI Console Tool:    ${BUILD_TUI}"
echo " Qt QML GUI:          ${BUILD_GUI}"
echo " Unit Tests:          ${BUILD_TESTS}"
if [ -n "${SYSROOT}" ]; then
    echo " Sysroot:             ${SYSROOT}"
fi
echo "======================================================================"

# Clean build if requested
if [ "${DO_CLEAN}" = "true" ] && [ -d "${BUILD_DIR}" ]; then
    echo -e "\n[*] Cleaning prior build artifacts in ${BUILD_DIR}..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

# ------------------------------------------------------------------------------
# 1. CMake Configuration
# ------------------------------------------------------------------------------
echo -e "\n[*] Configuring CMake for Raspberry Pi (Release)..."
"${CMAKE_CMD}" -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_GUI="${BUILD_GUI}" \
    -DBUILD_SERVER="${BUILD_SERVER}" \
    -DBUILD_TUI="${BUILD_TUI}" \
    -DBUILD_TESTS="${BUILD_TESTS}" \
    "${CMAKE_EXTRA_ARGS[@]}"

# ------------------------------------------------------------------------------
# 2. Build Targets
# ------------------------------------------------------------------------------
echo -e "\n[*] Building targets (${JOBS} parallel jobs)..."
"${CMAKE_CMD}" --build "${BUILD_DIR}" --parallel "${JOBS}"

# ------------------------------------------------------------------------------
# 3. Stage Output Binaries & Data
# ------------------------------------------------------------------------------
echo -e "\n[*] Verifying and staging output binaries in ${BUILD_DIR}..."

BUILT_TARGETS=()
for bin in ir_api_server ir_tui ir_gui test_all; do
    src_bin=$(find "${BUILD_DIR}" -maxdepth 3 -type f -name "${bin}" ! -path "${BUILD_DIR}/${bin}" 2>/dev/null | head -n 1)
    if [ -n "${src_bin}" ]; then
        cp -fv "${src_bin}" "${BUILD_DIR}/${bin}"
        chmod +x "${BUILD_DIR}/${bin}"
        BUILT_TARGETS+=("${bin}")
    elif [ -f "${BUILD_DIR}/${bin}" ]; then
        chmod +x "${BUILD_DIR}/${bin}"
        BUILT_TARGETS+=("${bin}")
    fi
done

# Copy sample / default signal database if available
if [ -f "${SCRIPT_DIR}/saved_ir_signals.json" ]; then
    cp -fv "${SCRIPT_DIR}/saved_ir_signals.json" "${BUILD_DIR}/saved_ir_signals.json"
fi

# ------------------------------------------------------------------------------
# 4. Generate Raspberry Pi Deployment Helpers
# ------------------------------------------------------------------------------
echo -e "\n[*] Generating Raspberry Pi deployment configs in ${BUILD_DIR}..."

# 4a. udev rule for USB transceiver permissions without root
cat << 'EOF' > "${BUILD_DIR}/99-ir-transceiver.rules"
# Tiqiaa Tview USB IR Transceiver permissions
# Grants non-root users in plugdev/dialout full read/write access
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", ATTR{idProduct}=="8468", MODE="0666", GROUP="plugdev"
SUBSYSTEM=="usb-serial", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="8468", MODE="0666", GROUP="plugdev"
KERNEL=="ttyUSB*", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="8468", MODE="0666", GROUP="dialout"
EOF

# 4b. systemd service template for ir_api_server
cat << 'EOF' > "${BUILD_DIR}/ir_api_server.service"
[Unit]
Description=IR Transceiver REST API Server Daemon
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/ir-transceiver
ExecStart=/home/pi/ir-transceiver/ir_api_server --port 8080 --storage /home/pi/ir-transceiver/saved_ir_signals.json
Restart=on-failure
RestartSec=5
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
EOF

# 4c. Quick deployment instructions
cat << EOF > "${BUILD_DIR}/DEPLOY_README.txt"
================================================================================
 IR Transceiver Suite - Raspberry Pi Deployment Guide
 Target Architecture: ${TARGET_ARCH}
================================================================================

1. USB HARDWARE PERMISSIONS (udev)
   To allow running without 'sudo', copy the udev rule on your Raspberry Pi:
     sudo cp 99-ir-transceiver.rules /etc/udev/rules.d/
     sudo udevadm control --reload-rules && sudo udevadm trigger
     sudo usermod -aG plugdev,dialout \$USER

2. RUNNING THE INTERACTIVE TUI CLIENT
     ./ir_tui
   Or connect to a remote API server:
     ./ir_tui --api http://127.0.0.1:8080

3. RUNNING THE STANDALONE REST API SERVER
     ./ir_api_server --port 8080 --storage saved_ir_signals.json

4. RUNNING AS A SYSTEMD BACKGROUND SERVICE
     sudo cp ir_api_server.service /etc/systemd/system/
     sudo systemctl daemon-reload
     sudo systemctl enable --now ir_api_server.service
     sudo systemctl status ir_api_server.service

5. RUNNING THE TEST SUITE
     ./test_all
================================================================================
EOF

# ------------------------------------------------------------------------------
# 5. Run Test Suite
# ------------------------------------------------------------------------------
if [ "${BUILD_TESTS}" = "ON" ] && [ -f "${BUILD_DIR}/test_all" ]; then
    echo -e "\n[*] Checking test execution capability..."
    CAN_RUN_TESTS="false"

    if [ "${BUILD_MODE}" = "native" ]; then
        CAN_RUN_TESTS="true"
    else
        # Check if QEMU user emulation can run target binaries
        if command -v "qemu-${TARGET_ARCH}-static" &>/dev/null || \
           command -v "qemu-${TARGET_ARCH}" &>/dev/null; then
            if "${BUILD_DIR}/test_all" ring_buffer &>/dev/null; then
                CAN_RUN_TESTS="true"
                echo "[*] QEMU user emulation detected. Running tests via emulation..."
            fi
        fi
    fi

    if [ "${CAN_RUN_TESTS}" = "true" ]; then
        echo -e "\n[*] Running unified test suite..."
        ctest --test-dir "${BUILD_DIR}" --output-on-failure
    else
        echo "[*] Notice: Cross-compiled target cannot run directly on host architecture (${HOST_ARCH})."
        echo "    To execute the test suite, copy '${BUILD_DIR}/test_all' to your Raspberry Pi and run './test_all'."
    fi
fi

# ------------------------------------------------------------------------------
# 6. Create Distribution Tarball
# ------------------------------------------------------------------------------
PACKAGE_TARBALL="${BUILD_DIR}/ir-transceiver-pi-${TARGET_ARCH}.tar.gz"
if [ "${DO_PACKAGE}" = "true" ]; then
    echo -e "\n[*] Creating Raspberry Pi deployment bundle: ${PACKAGE_TARBALL} ..."
    
    PACKAGE_FILES=("saved_ir_signals.json" "99-ir-transceiver.rules" "ir_api_server.service" "DEPLOY_README.txt")
    for b in "${BUILT_TARGETS[@]}"; do
        if [ -f "${BUILD_DIR}/${b}" ]; then
            PACKAGE_FILES+=("${b}")
        fi
    done

    tar -czf "${PACKAGE_TARBALL}" -C "${BUILD_DIR}" "${PACKAGE_FILES[@]}"
    echo "[+] Archive successfully created: $(du -h "${PACKAGE_TARBALL}" | cut -f1)"
fi

# ------------------------------------------------------------------------------
# 7. Summary
# ------------------------------------------------------------------------------
echo -e "\n======================================================================"
echo " RASPBERRY PI BUILD COMPLETED SUCCESSFULLY!"
echo "======================================================================"
echo " Target Architecture: ${TARGET_ARCH} (${PI_DESC})"
echo " Staged Binaries in:   ${BUILD_DIR}"
for b in "${BUILT_TARGETS[@]}"; do
    echo "   - ${BUILD_DIR}/${b}"
done

if [ -f "${PACKAGE_TARBALL}" ]; then
    echo ""
    echo " Deployment Bundle:"
    echo "   - ${PACKAGE_TARBALL}"
    echo ""
    echo " Transfer to Raspberry Pi with:"
    echo "   scp ${PACKAGE_TARBALL} pi@<raspberry-pi-ip>:~"
    echo "   ssh pi@<raspberry-pi-ip> 'tar -xzf ir-transceiver-pi-${TARGET_ARCH}.tar.gz && ./ir_tui'"
fi
echo "======================================================================"
