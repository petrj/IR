#!/usr/bin/env bash
set -e

# Resolve repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "========================================================"
echo " Cleaning IR Transceiver Build Artifacts"
echo " Target Directory: ${BUILD_DIR}"
echo "========================================================"

if [ -d "${BUILD_DIR}" ]; then
    echo "[*] Deleting build directory: ${BUILD_DIR} ..."
    rm -rf "${BUILD_DIR}"
    echo "[+] Build directory deleted successfully."
else
    echo "[*] No build directory found. Clean state."
fi

# Remove temporary test files if present
rm -f /tmp/test_api_storage.json \
      /tmp/test_ir_storage_unit_test.json \
      /tmp/test_unified_storage.json \
      /tmp/test_api_unified.json

# Remove copied binaries from project root
rm -f "${SCRIPT_DIR}/ir_api_server" \
      "${SCRIPT_DIR}/ir_tui" \
      "${SCRIPT_DIR}/ir_tui_test" \
      "${SCRIPT_DIR}/test_all"

echo "========================================================"
echo " CLEAN COMPLETED SUCCESSFULLY!"
echo "========================================================"
