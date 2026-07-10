# Tiqiaa Tview IR Transceiver (Python Control)

This project contains a Python script for controlling the **Tiqiaa Tview** infrared (IR) transceiver (receiver and transmitter) over the USB interface using the `PyUSB` library.

---

## 🛠️ Device hardware identification (HW info)

When connected to a computer, the device reports the following identifiers:

| Parameter | Value | Note |
| :--- | :--- | :--- |
| **Device name** | Tiqiaa Tview USB IR Transceiver | Also often referred to as the *ZaZaRemote USB OTG dongle* or *Tiqiaa USB IR Blaster* |
| **Vendor ID (VID)** | `0x10C4` | Silicon Laboratories, Inc. (Cygnal Integrated Products) |
| **Product ID (PID)** | `0x8468` | Custom ID used for this IR device |
| **Integrated chip** | Silicon Labs custom USB HID | A modified version of the CP210x family, often reported as *SLAB HIDtoSPI Slave* or *HIDtoUART* |
| **USB Interface** | `0` | Main communication interface |
| **USB Endpoints** | `EP_OUT = 0x01`<br>`EP_IN = 0x81` | Write direction (transmit) and read direction (receive) |

> [!IMPORTANT]
> Because the device behaves as a proprietary USB HID (Human Interface Device) rather than a standard virtual serial port (COM/ttyUSB), the standard CP210x drivers will not work with it. The script communicates directly through the low-level USB interface.

---

## 📋 Project description

The project consists of two main files:
1. [tiqiaa_IR.py](tiqiaa_IR.py) – The main Python class and driver code implementing the Tiqiaa communication protocol (commands wrapped in `ST <id> <cmd> EN` with 56-byte fragmentation).
2. [run.sh](run.sh) – A simple launcher bash script.

### Main features:
* **Initialization and verification:** Establishing a connection with the device, detaching the kernel driver if present, and requesting the firmware version (`V`).
* **Reading mode (IR receive):** Switching the transceiver into receive mode (`R`), repeatedly polling for new IR data (`O`), decoding captured pulses (converting raw bytes to microseconds) and filtering out noise. The script then prints an array of pulses ready to be copied into the transmitter.
* **Sending mode (IR transmit):** Switching the transceiver into transmit mode (`S`), converting the given pulse lengths (in $\mu s$) into the device's native units (16 $\mu s$ blocks) and sending the signal (`D`) with a carrier frequency (e.g. 38 kHz).
* **Testing mode:** Detailed dump of the USB hardware information plus the receive loop with verbose decoding details.
* **REST API (web server):** An embedded HTTP server exposing JSON endpoints so that other computers on the network can read and send IR sequences remotely.

---

## 🚀 Getting started

### 1. Requirements
* **Python 3**
* **PyUSB** library and **libusb**. Install them from the standard system packages:
  ```bash
  sudo apt-get install python3-usb libusb-1.0-0
  ```
  The `python3-usb` package provides the `usb` module (PyUSB); `libusb-1.0-0` is the underlying USB backend.

### 2. Linux permissions (udev rule)
So that the script can access the USB device without running as `sudo`, create a udev rule:

1. Create the file `/etc/udev/rules.d/99-tiqiaa.rules` with the following content:
   ```udev
   SUBSYSTEM=="usb", ATTR{idVendor}="10c4", ATTR{idProduct}="8468", MODE="0666", GROUP="plugdev"
   ```
2. Reload the rules:
   ```bash
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ```
3. Make sure your user is in the `plugdev` group.

### 3. Running
Run the script using the included shell script:
```bash
./run.sh
```

You will be presented with an interactive menu:
```
=== MAIN MENU ===
 R) Reading mode
 S) Sending mode
 T) Testing mode
 W) Web server (REST API)
 Q) Quit
```

---

## ⌨️ Command-line usage

You can skip the menu and run a specific mode directly:

```bash
# Reading mode (capture IR codes)
python3 tiqiaa_IR.py --mode reading

# Sending mode (interactive transmit)
python3 tiqiaa_IR.py --mode sending

# Testing mode (diagnostics + verbose receive)
python3 tiqiaa_IR.py --mode testing

# Transmit a single sequence and exit
python3 tiqiaa_IR.py --send "9000, -4500, 560, -560, 560, -1690"

# Set a different default transmit frequency (kHz)
python3 tiqiaa_IR.py --frequency 38

# Start the REST API server directly
python3 tiqiaa_IR.py --api --host 0.0.0.0 --port 8080
```

---

## 🌐 REST API (web server)

The script can expose an embedded HTTP server that transfers data as JSON, allowing you to read and send IR sequences from another computer on the network.

Start it either from the menu (option `W`) or from the command line:

```bash
python3 tiqiaa_IR.py --api --host 0.0.0.0 --port 8080
```

> [!WARNING]
> By default the server binds to `127.0.0.1` (localhost only). Binding to `0.0.0.0` exposes IR send/receive to anyone who can reach the port on your network. There is no built-in authentication, so only expose it on a trusted network or place it behind a reverse proxy with access control.

### Endpoints

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api` | Short help / list of endpoints |
| `GET` | `/api/status` | Device information (manufacturer, product, VID/PID, serial, default frequency) |
| `GET` | `/api/read?timeout=10` | Capture a single IR sequence (waits up to `timeout` seconds) |
| `POST` | `/api/send` | Transmit an IR sequence. JSON body: `{"pulses": [...], "frequency": 38}` |

### Examples (using `curl`)

Replace `HOST` with the IP address or hostname of the machine running the server.

**Get device status:**
```bash
curl http://HOST:8080/api/status
```

**Capture (read) an IR code — press the remote button within the timeout:**
```bash
curl "http://HOST:8080/api/read?timeout=15"
```
Example response:
```json
{
  "pulses": [9000, -4500, 560, -560, 560, -1690],
  "count": 6
}
```

**Send (transmit) an IR sequence:**
```bash
curl -X POST http://HOST:8080/api/send \
     -H "Content-Type: application/json" \
     -d '{"pulses": [9000, -4500, 560, -560, 560, -1690], "frequency": 38}'
```
Example response:
```json
{
  "status": "sent",
  "count": 6,
  "frequency": 38
}
```

### Example: Python client

```python
import requests

BASE = "http://HOST:8080"

# Capture an IR code
resp = requests.get(f"{BASE}/api/read", params={"timeout": 15})
pulses = resp.json()["pulses"]
print("Captured:", pulses)

# Replay the same code
requests.post(f"{BASE}/api/send", json={"pulses": pulses, "frequency": 38})
```

---

## 🔗 Useful resources

* [Reverse engineering of the Tiqiaa Tview protocol (Habr)](https://habr.com/ru/articles/494800/) – A detailed analysis and breakdown of the USB communication and protocol of this IR transceiver.

---

## 📎 Example captured codes

**Works on the TV (Volume UP):**
```
9008, -4560, 544, -608, 528, -640, 512, -608, 528, -608, 544, -592, 544, -608, 528, -608, 528, -640, 512, -1728, 544, -1728, 544, -1728, 560, -1712, 544, -1728, 544, -608, 528, -1728, 544, -1728, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -1728, 544, -608, 544, -1728, 544, -608, 528, -608, 528, -1728, 544, -608, 544, -608, 528, -608, 544, -1728, 544, -608, 560, -1712, 560, -40672, 8992, -2288, 544, -304
```

**Works on the Sharp Repro (EQ button):**
```
6944, -4576, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -608, 544, -608, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -1728, 592, -1680, 592, -1680, 592, -1680, 592, -560, 592, -1680, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -1728, 544, -40688, 9008, -2288, 592, -304
```
