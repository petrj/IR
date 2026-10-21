# Multi-Platform IR Transceiver Application

A high-performance, cross-platform C++ / Qt 6 application for receiving, transmitting, and processing Infra-Red (IR) signals using USB transceiver hardware.

This project is a lightweight, native C++ engine (`libusb`), providing an embedded HTTP REST API server (`ir_api_server`), a modern graphical interface (**QML**), and an interactive terminal / console test tool (**`ir_tui`**) that connects directly to the HTTP REST API.

---

## 🏗 Architecture Overview

The system architecture decouples the frontends from the hardware drivers via an embedded REST HTTP API server, which acts as the central interface to the native C++ engine ([`ir_core`](src/core/README.md)):

```
+-----------------------------------+     +---------------------------------+
|    QML Graphical User Interface   |     |     Console Test Tool (TUI)     |
|  (Windows / Linux / Android GUI)  |     |       (ir_tui / CLI)       |
+-----------------+-----------------+     +----------------+----------------+
                  |                                        |
                  |         HTTP REST API (JSON)           |
                  +-------------------+--------------------+
                                      |
                                      v
                      +---------------------------------+
                      |     Embedded REST HTTP API      |
                      |  (ir_api_server / cpp-httplib)  |
                      +---------------+-----------------+
                                      |
                                      v
                      +---------------------------------+
                      |      IR Core Engine (C++)       |
                      |  - IR signal timing/pulses      |
                      |  - Lock-free ring buffer        |
                      +---------------+-----------------+
                                      |
                                      v
                      +---------------------------------+
                      |         libusb-1.0              |
                      |  (Cross-platform USB I/O)       |
                      +---------------------------------+
```

---

## 🚀 Key Features

* **Cross-Platform Support:** Native operation across **Linux**, **Windows**, and **Android**.
* **Direct Hardware Access:** Ultra-low latency USB communication via native C++ `libusb-1.0`.
* **Dual Frontend Architecture:**
  * **QML / Qt Quick UI:** Modern graphical interface optimized for desktop and mobile touchscreens, interacting with the transceiver via the Embedded REST HTTP API.
  * **Terminal Console Tool (`ir_tui`):** Full-featured interactive console dashboard and CLI tool communicating over the HTTP REST API (`http://127.0.0.1:8080`) for rapid hardware debugging, protocol inspection, single capture, continuous polling, transmission, signal property editing, and manual signal creation.
* **Embedded REST HTTP API:** Trigger IR transmissions, capture pulses, or monitor hardware status remotely via standard HTTP endpoints ([Documentation](src/api/README.md)).
* **Local JSON Signal Database & Automation:** The TUI and QML apps manage a local JSON database organized by Remote Control Devices (RCD), supporting pulse sequences, carrier frequency, protocol identification, and optional shell command execution triggers when signals are matched.
* **Asynchronous Driver Execution:** USB polling runs on dedicated worker threads (`QThread` / `std::thread`), keeping all UI interaction smooth and non-blocking.

---

## 🛠 Project Structure

```
IR-QtApp/
├── CMakeLists.txt                # Root build configuration
├── README.md                     # Project documentation
├── saved_ir_signals.json         # Local fallback for the app's JSON signal database
├── src/
│   ├── core/                     # Shared static library (ir_core)
│   │   ├── README.md             # Library documentation & API reference
│   │   ├── IrUsbTransceiver.cpp  # Core libusb hardware engine
│   │   ├── IrUsbTransceiver.h
│   │   ├── IrProtocol.cpp        # Protocol encoding/decoding implementation
│   │   ├── IrProtocol.h          # Signal encoding/decoding definitions
│   │   ├── RingBuffer.h          # Thread-safe buffer for raw IR pulses
│   │   ├── SignalStorage.cpp     # JSON persistence for signals & Remote Control Devices
│   │   └── SignalStorage.h
│   ├── api/                      # Embedded Web API (ir_api)
│   │   ├── README.md             # REST API reference and client examples
│   │   ├── HttpServerModule.cpp  # REST HTTP endpoints
│   │   ├── HttpServerModule.h
│   │   └── main_api.cpp          # Standalone REST API server executable
│   ├── ui_qml/                   # Graphical GUI
│   │   ├── main.cpp              # QML entry point
│   │   ├── AppController.cpp     # QObject bridge for QML
│   │   ├── AppController.h
│   │   ├── main.qml              # Declarative UI layout
│   │   ├── ManualAddPopup.qml    # Manual signal addition and editing popup
│   │   ├── CapturePopup.qml      # Signal capture and save popup
│   │   ├── TriggerPopup.qml      # Trigger Mode modal with armed list & live execution log
│   │   └── TopBar.qml            # Status and connection header
│   └── ui_tui/                   # Terminal GUI Test Tool (HTTP API Consumer)
│       ├── main_tui.cpp          # Console entry point & CLI parser
│       ├── TuiController.cpp     # Interactive console controller & editor
│       └── TuiController.h
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
* **HTTP Server / Client Library:** [cpp-httplib](https://github.com/yhirose/cpp-httplib) (`libcpp-httplib-dev`)
* **JSON Library:** [nlohmann-json](https://github.com/nlohmann/json) (`nlohmann-json3-dev`)

### Installing Dependencies

#### Host Ubuntu / Debian (.deb Packages for Desktop)
Run the following command to install all required dependencies for the full project on your host desktop:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  qtbase5-dev qttools5-dev qt5-default \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools \
  libusb-1.0-0-dev \
  libcpp-httplib-dev \
  nlohmann-json3-dev
```

#### Raspberry Pi & ARM Cross-Compilation Packages (.deb)
To cross-compile from your x86_64 Debian/Ubuntu PC for Raspberry Pi and similar ARM devices using `./build-release-pi.sh`:

##### 1. For 64-bit Raspberry Pi (RPi 5, 4, 3, Zero 2 W, Orange Pi, Rock Pi, Odroid):
```bash
# Cross-compiler toolchain
sudo apt update
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Hardware USB transceiver library (libusb multiarch for arm64)
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install -y libusb-1.0-0-dev:arm64

# (Optional) QEMU user emulation to run the test suite directly on your PC:
sudo apt install -y qemu-user-static
```

##### 2. For 32-bit Raspberry Pi (RPi 2, 3, 4 with 32-bit Raspberry Pi OS):
```bash
# Cross-compiler toolchain
sudo apt update
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Hardware USB transceiver library (libusb multiarch for armhf)
sudo dpkg --add-architecture armhf
sudo apt update
sudo apt install -y libusb-1.0-0-dev:armhf
```

#### Native Compilation on Raspberry Pi OS (.deb)
If building directly on your Raspberry Pi (or similar ARM SBC) via `./build-release-pi.sh --native`:

##### Headless Setup (REST API Server + Console TUI - No Qt required):
```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  libusb-1.0-0-dev \
  libcpp-httplib-dev \
  nlohmann-json3-dev
```

##### Full Setup with Qt QML GUI (on Raspberry Pi OS Desktop):
```bash
sudo apt install -y \
  build-essential \
  cmake \
  libusb-1.0-0-dev \
  libcpp-httplib-dev \
  nlohmann-json3-dev \
  qt6-base-dev \
  qt6-declarative-dev
```

> If you are using the Qt 6 toolchain, the essential GUI/debug packages are `qt6-base-dev`, `qt6-declarative-dev`, and `qt6-tools-dev`. The `qt5` packages are only needed for older Qt 5 compatibility setups and are not required for this project unless you intentionally build against Qt 5.

#### Debian Package Breakdown:
| Package (.deb) | Purpose |
| :--- | :--- |
| `build-essential` | C/C++ compiler toolchain (GCC/G++, Make) |
| `cmake` | Cross-platform build system |
| `qt6-base-dev` | Qt 6 core libraries and development tools for the GUI app |
| `qt6-declarative-dev` | Qt 6 QML / Quick UI modules |
| `qt6-tools-dev` | Qt Creator integration and Qt tooling support |
| `qt6-tools-dev-tools` | Extra Qt toolchain utilities used by IDEs/debuggers |
| `libusb-1.0-0-dev` | C headers and development files for direct USB communication |
| `libcpp-httplib-dev` | C++17 HTTP/HTTPS server and client library for the embedded REST API |
| `nlohmann-json3-dev` | Modern C++ JSON parser and serializer for REST requests/responses |

#### Qt Creator / Debugging Notes
To debug the GUI target directly in Qt Creator, make sure the following are installed on the system:

```bash
sudo apt install -y qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools
```

This ensures Qt Creator can find `Qt6Config.cmake` and attach the debugger to the `ir_gui` target with the project’s CMake configuration.

#### Windows (vcpkg / MSVC)
```cmd
vcpkg install libusb:x64-windows cpp-httplib:x64-windows nlohmann-json:x64-windows qtbase:x64-windows qtdeclarative:x64-windows
```

---

## ⚙️ Building the Application

### 1. Clone the Repository
```bash
git clone https://github.com/petrj/IR.git
cd IR
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

# Build the Qt Graphical User Interface
cmake --build . --target ir_gui

# Build the TUI test tool
cmake --build . --target ir_tui

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

# Build for Raspberry Pi & ARM Single-Board Computers (build/pi-release)
# Supports Raspberry Pi 3/4/5, Zero 2 W, Orange Pi, Rock Pi, Banana Pi, Odroid, etc.
./build-release-pi.sh                         # Standard 64-bit ARM build (auto-detects native/cross)
./build-release-pi.sh --headless              # Headless build (server + TUI, no Qt required)
./build-release-pi.sh --arch armhf            # 32-bit ARM / Raspberry Pi OS build
./build-release-pi.sh --native                # Force native compilation when on the device
./build-release-pi.sh --sysroot /mnt/rpi-root # Cross-compile using a target sysroot

# Build for Android
./build-android-debug.sh
./build-android-release.sh

# Clean all build artifacts and delete the build/ directory
./clean.sh
```

---

## 💻 Terminal Console Tool (`ir_tui`)

The `ir_tui` executable provides an interactive console menu and CLI commands that interface directly with the hardware through the embedded REST HTTP API:

```bash
# 1. Run interactive menu (connects to http://127.0.0.1:8080 by default)
./build/debug/ir_tui

# 2. Connect to an API server running on a custom host/port
./build/debug/ir_tui --api http://127.0.0.1:8089

# 3. Single-shot capture via HTTP API and exit (exit code 0 on success)
./build/debug/ir_tui --read

# 4. Transmit a signal via API (by stored name, index, or pulse timings)
./build/debug/ir_tui --send "TV Power"
./build/debug/ir_tui --send "[9000, -4500, 560, -560, 560, -1690]"

# 5. List all locally stored and recognized signals
./build/debug/ir_tui --list

# 6. Display hardware diagnostics and connection status via API
./build/debug/ir_tui --info

# 7. Start Trigger Mode (listen for signals with defined shell commands & execute them)
./build/debug/ir_tui --trigger
```

### Interactive Menu Features

When run interactively (`./build/debug/ir_tui`), the console dashboard provides:
* **1) Capture IR Signal:** Single-shot capture with prompt to assign a friendly name, Remote Control Device (RCD), and optional shell command.
* **2) Continuous Monitor Mode:** Real-time polling stream. Automatically identifies known signals and **executes associated shell commands** when recognized.
* **3) Save Last Captured Signal:** Commit the last captured signal to local JSON storage.
* **4) List & Manage Saved Signals:** Full signal overview grouped by Remote Control Device. Supports interactive actions:
  * `E #` — **Edit all properties** of signal `#` (Name, RCD, Protocol, Frequency, Raw Hex, Pulses, Shell Command).
  * `D #` — **Delete** signal `#` (automatically prunes empty RCDs).
  * `A` — **Add** a new signal manually.
* **5) Transmit IR Signal:** Transmit saved signals, presets, or custom pulse arrays through the API.
* **6) Hardware Diagnostics:** View USB transceiver descriptors, firmware version, and server health.
* **7) Simulate IR Capture:** Synthesize NEC signals to verify decoding, matching, and shell command triggers without hardware.
* **8) Add Signal Manually:** Guided wizard to manually create and store a signal with custom pulse sequences or NEC hex codes and an optional shell command.
* **9) Trigger Mode:** Automated background listening loop. Arms all saved signals that have a configured shell command, executes the corresponding shell command whenever an armed signal is received, and continuously waits for the next command until stopped.

> [!TIP]
> You can also configure the target API server URL using the `IR_API_URL` environment variable:
> ```bash
> export IR_API_URL="http://192.168.1.100:8080"
> ./build/debug/ir_tui
> ```

---

## ⚡ Trigger Mode (Automation & Shell Execution)

**Trigger Mode** transforms your computer into an automated IR-controlled command center.

### Workflow:
1. **Arming Triggers**: When Trigger Mode is started, the application loads all stored signals and identifies every signal configured with an associated **shell command** (e.g., `pactl set-sink-volume @DEFAULT_SINK@ +5%`, `playerctl play-pause`, or any custom script).
2. **Waiting for Signal**: The app actively monitors incoming IR signals via the transceiver.
3. **Command Execution**: As soon as a user presses an armed button on their remote control:
   * The signal is identified and matched against the local signal catalog.
   * Its configured shell command is automatically executed.
4. **Continuous Operation**: After executing the command, the app **immediately resumes waiting for the next trigger command** in an ongoing loop. Unarmed signals and unrecognized noise are ignored without interrupting the listening cycle.

### Using Trigger Mode in TUI (`ir_tui`)
* **Interactive Menu**: Run `./build/debug/ir_tui` and select Option `9) Trigger Mode`.
* **Direct CLI Launch**: Run `./build/debug/ir_tui --trigger` (or `-t`).
* **Exit**: Press `ENTER` at any time to return to the menu.

### Using Trigger Mode in GUI (`ir_gui`)
* Click the **`⚡ Trigger Mode`** button in the top toolbar to open the **Trigger Mode** control modal.
* The modal provides:
  * **Real-time Status**: Shows active/inactive state and count of armed triggers.
  * **Armed Triggers Panel**: Lists every signal armed with a shell command (`cmd: <command>`).
  * **Live Activity Log**: Streams every trigger detection, execution event, and timestamp.
  * **`▶ Start Trigger Mode` / `⏹ Stop Trigger Mode`**: Start or stop automated listening with a single click.
  * **Background Execution**: You can click "Run in Background" (or close the modal) to keep Trigger Mode running while you browse signals. A `Trigger Active (N)` status badge appears in the top bar to indicate background operation.

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

Script example:
```
V="10c4"; P="8468"; \
echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="'$V'", ATTRS{idProduct}=="'$P'", MODE="0666", GROUP="plugdev"' \
| sudo tee /etc/udev/rules.d/99-usb-$V-$P.rules > /dev/null && \
sudo usermod -aG plugdev $USER && \
sudo udevadm control --reload-rules && \
sudo udevadm trigger
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