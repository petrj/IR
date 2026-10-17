# `ir_gui` — QML Signal Grid & Hardware Control UI

The Qt Quick / QML GUI is the desktop-facing application for the IR transceiver. It connects to the embedded REST API for hardware operations, while using the shared core-layer `SignalStorage` implementation for the same local JSON database used by the TUI client.

## Purpose

The GUI focuses on two responsibilities:

1. Monitoring and controlling the USB IR device through the HTTP API.
2. Managing the local saved-signal catalog organized by **Remote Control Device (RCD)** through the shared core `SignalStorage` implementation used by the TUI.

This keeps the REST API hardware-focused while providing full remote control device management locally.

---

## UI Behaviour

### Main Page
The main screen shows a saved-signal list with one card per stored IR command, grouped and badged by its assigned Remote Control Device (RCD).

Each card displays:

- Remote Control Device (RCD) badge (e.g., `Default Remote Device`, `Living Room TV`)
- signal name
- protocol (`Raw` or `NEC`)
- pulse count
- frequency (kHz)
- timestamp

The toolbar includes:

- **Saved Signals** header
- **Refresh (↻)** button
- **Add Manual (+)** button
- **Capture (📡)** button
- **RCD Filter ComboBox**: Filter displayed signals by "All Devices" or a specific Remote Control Device.

### Add Manual
The manual-add popup allows the user to create a saved signal without hardware capture.

Supported fields:

- signal name
- **Remote Control Device (RCD)**: Select an existing RCD from dropdown or choose `+ New Remote Device...` to type a new device name
- protocol (`Raw` or `NEC`)
- frequency in kHz
- rawHex (optional)
- comma-separated pulse values (optional)

When saved, the entry is written to the local JSON file under the specified RCD.

### Capture via API
The capture flow starts a REST `POST /api/v1/capture` request. While the request is active, the UI shows a capture popup and waits for a remote button press.

When a valid signal arrives:

- the captured pulses are stored in memory and decoded if valid NEC
- **RCD Specification**: The user must specify the Remote Control Device (select an existing RCD from the dropdown or select `+ Create New Remote Device...` to input a new RCD name)
- the user specifies the signal name
- clicking "Save Signal" commits the signal to the specified RCD in the local JSON storage

### Remove Saved Signal
Each saved-signal card includes a Delete (🗑) button. The action calls `SignalStorage::removeSignal()`:
- The signal is removed from its Remote Control Device.
- In accordance with the domain constraint ($1 \dots n$ signals per RCD), if the removed signal was the last signal in that RCD, the RCD is automatically deleted.
- The JSON file is rewritten immediately.

### Transmit Saved Signal
Each saved-signal card includes a Send (➤) button. This transmits the signal through the hardware API via `POST /api/v1/send`.

---

## Storage Access Model

The GUI does not use a separate database from the TUI.

It instantiates `ir::SignalStorage` directly using the same default path logic:

- Linux: `~/.config/IR/saved_ir_signals.json`
- Windows: `%APPDATA%\IR\saved_ir_signals.json`

Both the QML app and the TUI share the same JSON file and the same Remote Control Device hierarchy. Legacy JSON files containing a flat signal list are automatically migrated to an RCD named `"Default Remote Device"` upon loading.

---

## Technical Implementation

### QML Files

- `main.qml` — main window, signal list, RCD filter, action buttons
- `TopBar.qml` — server and USB status indicators
- `ManualAddPopup.qml` — manual save form with RCD selector
- `CapturePopup.qml` — capture waiting and save/discard UX with existing/new RCD selector

### Controller Bridge

`AppController` acts as the bridge between QML and the C++ logic:

- reads the current API status from `/api/v1/status`
- exposes `rcdList` (`QStringList`) of existing device names
- exposes `selectedDeviceFilter` (`QString`) for filtering signals by device
- refreshes the signal grid from `SignalStorage`
- saves captured/manual signals to specified RCDs in the local JSON file
- deletes signals and prunes empty RCDs
- transmits a saved signal through the hardware API

### Behaviour Contract

- hardware state is API-driven
- signal and RCD data is local-file driven
- REST API remains hardware-focused and does not expose signal CRUD endpoints
- GUI and TUI are synchronized because they both read and write the same storage file

---

## Notes

This module is intended to be the desktop application UI for local IR signal and device management. The API remains the transport layer for hardware actions, while the JSON file remains the single source of truth for saved Remote Control Devices and commands.
