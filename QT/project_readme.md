# Multi-Platform IR Transceiver Application

A high-performance, cross-platform C++ / Qt 6 application for receiving, transmitting, and processing Infra-Red (IR) signals using USB transceiver hardware.

This project replaces legacy Python-based USB drivers with a lightweight, native C++ engine (`libusb`), providing an embedded HTTP REST API alongside two dedicated frontends: a modern graphical interface (**QML**) and a lightweight terminal interface (**FTXUI**).

---

## 🏗 Architecture Overview

The system is decoupled into a shared core library (`ir_core`) consumed by all frontends and services:

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
|   (QHttpServer / cpp-httplib)   |                     |
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
* **Embedded REST HTTP API:** Trigger IR transmissions or monitor status remotely via standard HTTP endpoints.
* **Asynchronous Driver Execution:** USB polling runs on dedicated worker threads (`QThread`), keeping all UI interaction smooth and non-blocking.

---

## 🛠 Project Structure

```
IR-QtApp/
├── CMakeLists.txt                # Root build configuration
├── README.md                     # Project documentation
├── src/
│   ├── core/                     # Shared static library (ir_core)
│   │   ├── IrUsbTransceiver.cpp  # Core libusb hardware engine
│   │   ├── IrUsbTransceiver.h
│   │   ├── IrProtocol.h          # Signal encoding/decoding definitions
│   │   └── RingBuffer.h          # Thread-safe buffer for raw IR pulses
│   ├── api/                      # Embedded Web API
│   │   ├── HttpServerModule.cpp  # REST HTTP endpoints
│   │   └── HttpServerModule.h
│   ├── ui_qml/                   # Graphical GUI
│   │   ├── main.cpp              # QML entry point
│   │   ├── AppController.cpp     # QObject bridge for QML
│   │   ├── AppController.h
│   │   └── main.qml              # Declarative UI layout
│   └── ui_tui/                   # Terminal GUI Test Tool
│       ├── main_tui.cpp          # FTXUI entry point
│       ├── TuiController.cpp     # Console UI controller
│       └── TuiController.h
└── android/                      # Android NDK & Java JNI files
    └── src/org/qtproject/example/UsbPermissionActivity.java
```

---

## 📦 Prerequisites & Dependencies

Ensure the following tools and libraries are installed on your build system:

* **Compiler:** C++17 compliant compiler (GCC 10+, Clang 12+, MSVC 2019+)
* **Build System:** CMake 3.16 or higher
* **Framework:** Qt 6.4+ (`Core`, `Network`, `Quick` modules)
* **USB Library:** `libusb-1.0`
* **Terminal UI Library:** [FTXUI](https://github.com/ArthurSpoon/FTXUI) (automatically fetched via CMake)

### Installing Dependencies

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev libusb-1.0-0-dev
```

#### Windows (vcpkg / MSVC)
```cmd
vcpkg install libusb:x64-windows qtbase:x64-windows qtdeclarative:x64-windows
```

---

## ⚙️ Building the Application

### 1. Clone the Repository
```bash
git clone https://github.com/your-username/IR-QtApp.git
cd IR-QtApp
```

### 2. Configure and Build

Build both the QML GUI and TUI executable:
```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

To build a specific binary:
```bash
# Build only the TUI test tool
cmake --build . --target ir_tui_test

# Build only the QML GUI application
cmake --build . --target ir_qml_app
```

---

## 🖥 Platform-Specific USB Configuration

### Linux
To access the USB hardware without root (`sudo`) privileges, create a `udev` rule:

```bash
# /etc/udev/rules.d/99-ir-usb.rules
SUBSYSTEM=="usb", ATTR{idVendor}=="1234", ATTR{idProduct}=="5678", MODE="0666", GROUP="plugdev"
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

The embedded API server listens by default on port `8080`.

| Method | Endpoint | Description | Example Payload |
| :--- | :--- | :--- | :--- |
| **GET** | `/api/v1/status` | Returns USB connection status | _None_ |
| **POST** | `/api/v1/send` | Transmits an IR signal | `{"protocol": "NEC", "code": "0x00FF45BA"}` |
| **GET** | `/api/v1/receive` | Reads the latest raw signal buffer | _None_ |

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.