#!/usr/bin/env bash
set -e

# Determine repository and target directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/debug"

echo "========================================================"
echo " Building IR Transceiver Application (DEBUG)"
echo " Project Source: ${SCRIPT_DIR}"
echo " Build Directory: ${BUILD_DIR}"
echo "========================================================"

mkdir -p "${BUILD_DIR}"

# 1. CMake configuration
echo -e "\n[*] Configuring CMake (Debug)..."
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug

# 2. Build all targets
JOBS=$(nproc 2>/dev/null || echo 4)
echo -e "\n[*] Building all targets (${JOBS} parallel jobs)..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

# 3. Copy the 3 built binaries directly into build/debug folder
echo -e "\n[*] Copying the 3 built binaries to ${BUILD_DIR} folder..."
for bin in ir_api_server ir_tui ir_gui test_all; do
    src_bin=$(find "${BUILD_DIR}" -type f -name "${bin}" ! -path "${BUILD_DIR}/${bin}" | head -n 1)
    if [ -n "${src_bin}" ]; then
        cp -fv "${src_bin}" "${BUILD_DIR}/${bin}"
    elif [ -f "${BUILD_DIR}/${bin}" ]; then
        echo "Binary ${bin} already located in ${BUILD_DIR}/${bin}"
    else
        echo "Warning: Binary ${bin} not found in build tree"
    fi
done

# 4. Run test suite
echo -e "\n[*] Running unified test suite..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo -e "\n========================================================"
echo " DEBUG BUILD & TESTS COMPLETED SUCCESSFULLY!"
echo " The binaries are located in: ${BUILD_DIR}"
echo "   - ${BUILD_DIR}/ir_api_server"
echo "   - ${BUILD_DIR}/ir_tui"
echo "   - ${BUILD_DIR}/ir_gui"
echo "   - ${BUILD_DIR}/test_all"
echo "========================================================"
