# Multi-Platform IR Transceiver Application

A high-performance, cross-platform C++ / Qt 6 application for receiving, transmitting, and processing Infra-Red (IR) signals using USB transceiver hardware.

This project replaces legacy Python-based USB drivers with a lightweight, native C++ engine (`libusb`), providing an embedded HTTP REST API alongside two dedicated frontends: a modern graphical interface (**QML**) and a lightweight terminal interface (**FTXUI**).

---

## 🏗 Architecture Overview

The system is decoupled into a shared core library ([`ir_core`](src/core/README.md)) consumed by all frontends and services:

```
                          +-----------------------------------+
                          |     QML Graphical User Interface   |
                          |  (Windows / Linux / Android GUI)  |
                          +-----------------+-----------------+
                                            |
                                            v
+---------------------------------+   +-----+-------------------------+
|     FTXUI Terminal Interface    |-->|      IR Core Engine (C++)     |
|   (Console / Hardware Testing)  |   |  - IR signal timing/pulses    |
+---------------------------------+   |  - Lock-free ring buffer      |
                                      +-----------------+-------------+
+---------------------------------+                     |
|     Embedded REST HTTP API      |---------------------+
|         (cpp-httplib)           |                     |
+---------------------------------+                     v
                                      +-------------------------------+
                                      |         libusb-1.0            |
                                      |  (Cross-platform USB I/O)     |
                                      +-------------------------------+
```

---

## 🚀 Key Features

* **Cross-Platform Support:** Native operation across **Linux**, **Windows**, and **Android**.
* **Direct Hardware Access:** Ultra-low latency USB communication via native C++ `libusb-1.0`.
* **Dual Frontend Architecture:**
  * **QML / Qt Quick UI:** Single-codebase graphical interface optimized for desktop and mobile touchscreens.
  * **FTXUI Terminal UI:** Full-featured console dashboard for rapid hardware debugging, protocol inspection, and headless test setups.
* **Embedded REST HTTP API:** Trigger IR transmissions, capture pulses, or monitor hardware status remotely via standard HTTP endpoints ([Documentation](src/api/README.md)).
* **Asynchronous Driver Execution:** USB polling runs on dedicated worker threads (`QThread` / `std::thread`), keeping all UI interaction smooth and non-blocking.

---

## 🛠 Project Structure

```
IR-QtApp/
├── CMakeLists.txt                # Root build configuration
├── project_readme.md             # Project documentation
├── saved_ir_signals.json         # Default saved IR signals database
├── src/
│   ├── core/                     # Shared static library (ir_core)
│   │   ├── README.md             # Library documentation & API reference
│   │   ├── IrUsbTransceiver.cpp  # Core libusb hardware engine
│   │   ├── IrUsbTransceiver.h
│   │   ├── IrProtocol.cpp        # Protocol encoding/decoding implementation
│   │   ├── IrProtocol.h          # Signal encoding/decoding definitions
│   │   └── RingBuffer.h          # Thread-safe buffer for raw IR pulses
│   ├── api/                      # Embedded Web API (ir_api)
│   │   ├── README.md             # REST API reference and client examples
│   │   ├── HttpServerModule.cpp  # REST HTTP endpoints
│   │   ├── HttpServerModule.h
│   │   └── main_api.cpp          # Standalone REST API server executable
│   ├── ui_qml/                   # Graphical GUI
│   │   ├── main.cpp              # QML entry point
│   │   ├── AppController.cpp     # QObject bridge for QML
│   │   ├── AppController.h
│   │   └── main.qml              # Declarative UI layout
│   └── ui_tui/                   # Terminal GUI Test Tool
│       ├── main_tui.cpp          # FTXUI entry point
│       ├── TuiController.cpp     # Console UI controller
│       ├── TuiController.h
│       ├── SignalStorage.cpp     # JSON persistence for remote signals
│       └── SignalStorage.h
├── tests/                        # Automated unit & integration tests
│   ├── CMakeLists.txt            # Test configuration (test_all target)
│   └── test_all.cpp              # Unified test suite (IR Core + REST API)
└── android/                      # Android NDK & Java JNI files
    └── src/org/qtproject/example/UsbPermissionActivity.java
```

---

## 📦 Prerequisites & Dependencies

Ensure the following tools and libraries are installed on your build system:

* **Compiler:** C++17 compliant compiler (GCC 10+, Clang 12+, MSVC 2019+)
* **Build System:** CMake 3.16 or higher
* **Framework:** Qt 6.4+ (`Core`, `Network`, `Quick` modules for GUI)
* **USB Library:** `libusb-1.0`
* **HTTP Server Library:** [cpp-httplib](https://github.com/yhirose/cpp-httplib) (`libcpp-httplib-dev`)
* **JSON Library:** [nlohmann-json](https://github.com/nlohmann/json) (`nlohmann-json3-dev`)
* **Terminal UI Library:** [FTXUI](https://github.com/ArthurSpoon/FTXUI) (automatically fetched via CMake)

### Installing Dependencies

#### Ubuntu / Debian (.deb Packages)
Run the following command to install all required dependencies:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev libusb-1.0-0-dev libcpp-httplib-dev nlohmann-json3-dev
```

#### Debian Package Breakdown:
| Package (.deb) | Purpose |
| :--- | :--- |
| `build-essential` | C/C++ compiler toolchain (GCC/G++, Make) |
| `cmake` | Cross-platform build system |
| `libusb-1.0-0-dev` | C headers and development files for direct USB communication |
| `libcpp-httplib-dev` | C++17 HTTP/HTTPS server and client library for the embedded REST API |
| `nlohmann-json3-dev` | Modern C++ JSON parser and serializer for REST requests/responses |
| `qt6-base-dev` | Qt 6 core libraries and development tools (for QML GUI) |
| `qt6-declarative-dev` | Qt 6 QML / Quick declarative UI components |

> [!NOTE]
> If `libcpp-httplib-dev` or `nlohmann-json3-dev` are not installed, CMake's `FetchContent` mechanism will automatically fetch and compile them from GitHub as a fallback during the build process.

#### Windows (vcpkg / MSVC)
```cmd
vcpkg install libusb:x64-windows cpp-httplib:x64-windows nlohmann-json:x64-windows qtbase:x64-windows qtdeclarative:x64-windows
```

---

## ⚙️ Building the Application

### 1. Clone the Repository
```bash
git clone https://github.com/your-username/IR-QtApp.git
cd IR-QtApp
```

### 2. Configure and Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build . --parallel
```

### 3. Build Specific Targets
```bash
# Build the REST API standalone server
cmake --build . --target ir_api_server

# Build the TUI test tool
cmake --build . --target ir_tui_test

# Build unified test suite (IR Core + REST API)
cmake --build . --target test_all

# Run all test suites via CTest
ctest --output-on-failure
```

### 4. Build & Cleanup Helper Scripts
Convenience scripts are provided in the project root:

```bash
# Build & test in Debug mode (build/debug)
./build-debug.sh

# Build & test in Release mode (build/release)
./build-release.sh

# Clean all build artifacts and delete the build/ directory
./clean.sh
```

---

## 🖥 Platform-Specific USB Configuration

### Linux
To access the USB hardware without root (`sudo`) privileges, create a `udev` rule:

```bash
# /etc/udev/rules.d/99-ir-usb.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", ATTR{idProduct}=="8468", MODE="0666", GROUP="plugdev"
```
Reload rules:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Windows
Ensure the USB transceiver uses the **WinUSB** driver. Windows 10/11 will typically match WinUSB automatically; otherwise, use [Zadig](https://zadig.akeo.ie/) to assign the WinUSB driver to the hardware endpoint.

### Android
Due to SELinux restrictions, Android applications cannot directly call `libusb_open()`. 

1. Request USB permission using Android's native `UsbManager`.
2. Retrieve the integer file descriptor via `UsbDeviceConnection.getFileDescriptor()`.
3. Pass the descriptor to C++ through JNI and wrap it with `libusb`:
   ```cpp
   libusb_wrap_sys_device(nullptr, (intptr_t)android_fd, &dev_handle);
   ```

---

## 📡 HTTP REST API Reference

The embedded API server listens by default on port `8080`. For full documentation, examples (cURL, Python, JS, C++), and schemas, see [`src/api/README.md`](src/api/README.md).

| Method | Endpoint | Description | Example Payload |
| :--- | :--- | :--- | :--- |
| **GET** | `/` | Service description and registered endpoints | _None_ |
| **GET** | `/api/v1/status` | Returns USB connection & device status | _None_ |
| **POST** | `/api/v1/send` | Transmits an IR signal | `{"protocol": "NEC", "code": "0x00FF45BA"}` |
| **GET** | `/api/v1/receive` | Reads latest signal from receive ring buffer | _None_ (supports `?last=true`) |
| **POST** | `/api/v1/capture` | Synchronously waits for incoming signal | `{"timeout": 5.0}` |
| **POST** | `/api/v1/mode` | Sets transceiver operating mode | `{"mode": "idle"}` |
| **GET** | `/api/v1/signals` | Lists all stored signals | _None_ |
| **POST** | `/api/v1/signals` | Saves a new signal to storage | `{"name": "Power", "protocol": "NEC", ...}` |
| **GET** | `/api/v1/signals/:index` | Retrieves single signal by index | _None_ |
| **DELETE**| `/api/v1/signals/:index` | Deletes signal from storage by index | _None_ |
| **POST** | `/api/v1/signals/:index/send` | Transmits stored signal by index | _None_ |

### Quick Start Example
```bash
# Start the API server
./build/ir_api_server --port 8080

# Check status
curl -s http://localhost:8080/api/v1/status | jq .

# Send a NEC command
curl -X POST http://localhost:8080/api/v1/send \
     -H "Content-Type: application/json" \
     -d '{"protocol": "NEC", "code": "0x00FF45BA"}'
```

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.