#!/usr/bin/env bash
set -e

# ==============================================================================
# IR Transceiver Application - Android Build Script (DEBUG)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/android-debug"

# 1. Detect Android SDK
if [ -z "${ANDROID_SDK_ROOT}" ] && [ -n "${ANDROID_HOME}" ]; then
    ANDROID_SDK_ROOT="${ANDROID_HOME}"
fi
if [ -z "${ANDROID_SDK_ROOT}" ]; then
    for candidate in "${HOME}/Android/Sdk" "/opt/android-sdk" "${HOME}/android-sdk"; do
        if [ -d "${candidate}" ]; then
            ANDROID_SDK_ROOT="${candidate}"
            break
        fi
    done
fi

if [ -z "${ANDROID_SDK_ROOT}" ] || [ ! -d "${ANDROID_SDK_ROOT}" ]; then
    echo -e "\n[ERROR] Android SDK not found!"
    echo "Please set ANDROID_SDK_ROOT or ANDROID_HOME environment variable."
    exit 1
fi
export ANDROID_SDK_ROOT
export ANDROID_HOME="${ANDROID_SDK_ROOT}"

# 2. Detect Android NDK
if [ -z "${ANDROID_NDK_ROOT}" ] && [ -n "${ANDROID_NDK_HOME}" ]; then
    ANDROID_NDK_ROOT="${ANDROID_NDK_HOME}"
fi
if [ -z "${ANDROID_NDK_ROOT}" ] && [ -n "${ANDROID_NDK}" ]; then
    ANDROID_NDK_ROOT="${ANDROID_NDK}"
fi
if [ -z "${ANDROID_NDK_ROOT}" ] && [ -d "${ANDROID_SDK_ROOT}/ndk" ]; then
    LATEST_NDK=$(ls -1d "${ANDROID_SDK_ROOT}/ndk/"* 2>/dev/null | sort -V | tail -n 1)
    if [ -n "${LATEST_NDK}" ] && [ -d "${LATEST_NDK}" ]; then
        ANDROID_NDK_ROOT="${LATEST_NDK}"
    fi
fi

if [ -z "${ANDROID_NDK_ROOT}" ] || [ ! -d "${ANDROID_NDK_ROOT}" ]; then
    echo -e "\n[ERROR] Android NDK not found!"
    echo "Please set ANDROID_NDK_ROOT or install NDK via Android SDK Manager."
    exit 1
fi
export ANDROID_NDK_ROOT

# 3. Target ABI and Platform
ANDROID_ABI="${ANDROID_ABI:-arm64-v8a}"

if [ -z "${ANDROID_PLATFORM}" ]; then
    MAX_NDK_API=$(ls -1 "${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android" 2>/dev/null | grep -E '^[0-9]+$' | sort -n | tail -n 1)
    if [ -n "${MAX_NDK_API}" ]; then
        ANDROID_PLATFORM="android-${MAX_NDK_API}"
    else
        ANDROID_PLATFORM="android-35"
    fi
fi

# 4. Detect Java JDK
if [ -z "${JAVA_HOME}" ]; then
    if [ -d "/usr/lib/jvm/default-java" ]; then
        export JAVA_HOME="/usr/lib/jvm/default-java"
    elif [ -d "/usr/lib/jvm/java-21-openjdk-amd64" ]; then
        export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
    elif [ -d "/usr/lib/jvm/java-17-openjdk-amd64" ]; then
        export JAVA_HOME="/usr/lib/jvm/java-17-openjdk-amd64"
    fi
fi

# 5. Detect Qt for Android
QT_TOOLCHAIN_CMAKE=""
if [ -z "${QT_ROOT}" ] && [ -n "${QTDIR}" ]; then
    QT_ROOT="${QTDIR}"
fi

ABI_UNDERSCORE="${ANDROID_ABI//-/_}"
if [ -z "${QT_ROOT}" ]; then
    for candidate in \
        "${HOME}/Qt"/*"/android_${ABI_UNDERSCORE}" \
        "${HOME}/Qt"/*"/android" \
        "/opt/Qt"/*"/android_${ABI_UNDERSCORE}" \
        "/opt/Qt"/*"/android"; do
        if [ -d "${candidate}" ]; then
            QT_ROOT="${candidate}"
            break
        fi
    done
fi

CMAKE_CMD="cmake"
CMAKE_EXTRA_ARGS=()

if [ -n "${QT_ROOT}" ] && [ -f "${QT_ROOT}/bin/qt-cmake" ]; then
    CMAKE_CMD="${QT_ROOT}/bin/qt-cmake"
    echo "[*] Using Qt Android CMake wrapper: ${CMAKE_CMD}"
else
    CMAKE_EXTRA_ARGS+=(
        "-DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake"
    )
    if [ -n "${QT_ROOT}" ]; then
        CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=${QT_ROOT}")
    fi
fi

echo "========================================================"
echo " Building IR Transceiver Application (ANDROID DEBUG)"
echo " Project Source:   ${SCRIPT_DIR}"
echo " Build Directory:  ${BUILD_DIR}"
echo " Android SDK:      ${ANDROID_SDK_ROOT}"
echo " Android NDK:      ${ANDROID_NDK_ROOT}"
echo " Target ABI:       ${ANDROID_ABI}"
echo " Platform Level:   ${ANDROID_PLATFORM}"
if [ -n "${QT_ROOT}" ]; then
    echo " Qt Root:          ${QT_ROOT}"
fi
echo "========================================================"

mkdir -p "${BUILD_DIR}"

# 6. Configure with CMake
echo -e "\n[*] Configuring CMake for Android (Debug)..."
"${CMAKE_CMD}" -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DANDROID=1 \
    -DANDROID_ABI="${ANDROID_ABI}" \
    -DANDROID_PLATFORM="${ANDROID_PLATFORM}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_GUI=ON \
    -DBUILD_SERVER=OFF \
    -DBUILD_TUI=OFF \
    -DBUILD_TESTS=OFF \
    "${CMAKE_EXTRA_ARGS[@]}"

# 7. Build target
JOBS=$(nproc 2>/dev/null || echo 4)
echo -e "\n[*] Building Android targets (${JOBS} parallel jobs)..."
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

# 8. Attempt APK generation if targets exist
if cmake --build "${BUILD_DIR}" --target ir_gui_make_apk 2>/dev/null || \
   cmake --build "${BUILD_DIR}" --target apk 2>/dev/null; then
    echo -e "\n[*] Android APK package generated successfully."
fi

echo -e "\n========================================================"
echo " ANDROID DEBUG BUILD FINISHED!"
echo " Build artifacts in: ${BUILD_DIR}"
find "${BUILD_DIR}" -maxdepth 5 -name "libir_gui*.so" -o -name "*.apk" 2>/dev/null || true
echo "========================================================"
