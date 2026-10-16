# `ir_gui` — QML Signal Grid & Hardware Control UI

The Qt Quick / QML GUI is the desktop-facing application for the IR transceiver. It connects to the embedded REST API for hardware operations, while using the shared core-layer `SignalStorage` implementation for the same local JSON database used by the TUI client.

## Purpose

The GUI focuses on two responsibilities:

1. Monitoring and controlling the USB IR device through the HTTP API.
2. Managing the local saved-signal catalog through the shared core `SignalStorage` implementation used by the TUI.

This keeps the REST API hardware-only and avoids any network CRUD surface for saved signals.

---

## UI Behaviour

### Main Page
The main screen shows a saved-signal grid with one card per stored IR command.

Each card displays:

- signal name
- protocol
- pulse count
- frequency
- timestamp

The toolbar includes actions for:

- Refresh
- Add Manual
- Capture via API

### Add Manual
The manual-add popup allows the user to create a saved signal without hardware capture.

Supported fields:

- name
- protocol (`Raw` or `NEC`)
- frequency in kHz
- rawHex (optional)
- comma-separated pulse values (optional)

When saved, the entry is written to the same JSON file as the TUI storage layer.

### Capture via API
The capture flow starts a REST `POST /api/v1/capture` request. While the request is active, the UI shows a capture popup and waits for a remote button press.

When a valid signal arrives:

- the captured pulses are stored in memory
- the user can name the signal
- the signal can be saved locally to the JSON file

### Remove Saved Signal
Each saved-signal card includes a Delete button. The action calls the same `SignalStorage::removeSignal()` logic as the TUI, and the JSON file is rewritten immediately.

### Transmit Saved Signal
Each saved-signal card also includes a Send button. This sends the saved signal name to the API via `POST /api/v1/send` using the REST API endpoint rather than a direct storage operation.

---

## Storage Access Model

The GUI does not use a separate database from the TUI.

It instantiates `ir::SignalStorage` directly using the same default path logic:

- Linux: `~/.config/IR/saved_ir_signals.json`
- Windows: `%APPDATA%\IR\saved_ir_signals.json`

This means the QML app and the TUI share the same saved IR catalog and the same JSON file.

---

## Technical Implementation

### QML Files

- `main.qml` — main window, signal grid, action buttons
- `TopBar.qml` — server and USB status indicators
- `ManualAddPopup.qml` — manual save form
- `CapturePopup.qml` — capture waiting and save/discard UX

### Controller Bridge

`AppController` acts as the bridge between QML and the C++ logic:

- reads the current API status from `/api/v1/status`
- refreshes the signal grid from `SignalStorage`
- saves captured/manual signals to the same local JSON file
- deletes signals from the same local storage
- transmits a named saved signal through the hardware API

### Behaviour Contract

- hardware state is API-driven
- signal data is local-file driven
- REST API remains hardware-only and does not expose signal CRUD endpoints
- GUI and TUI are synchronized because they both read and write the same storage file

---

## Notes

This module is intended to be the desktop application UI for local IR signal management. The API remains the transport layer for hardware actions, while the JSON file remains the single source of truth for saved remote commands.
