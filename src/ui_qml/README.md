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
- **Shell command**: highlighted in cyan (`cmd: <shellCommand>`) if configured for the signal

Each card includes three action buttons:
- **Edit (✎)**: Opens the edit dialog preloaded with all signal properties (name, RCD, protocol, frequency, rawHex, pulses, shell command) for in-place modification.
- **Transmit (➤)**: Transmits the signal through the hardware API via `POST /api/v1/send`.
- **Delete (🗑)**: Prompts for confirmation and calls `SignalStorage::removeSignal()` (automatically pruning the RCD if it was the last command).

The toolbar includes:

- **Saved Signals** header
- **Refresh (↻)** button
- **Add Manual (+)** button
- **Capture (📡)** button
- **Trigger Mode (⚡)** button: Opens the automated Trigger Mode modal and indicates whether trigger listening is active
- **RCD Filter ComboBox**: Filter displayed signals by "All Devices" or a specific Remote Control Device.

### Add Manual & Edit Signal

The popup (`ManualAddPopup.qml`) serves a dual role:
1. **Add Manually**: Opened via the toolbar `+` button to create a new IR signal without hardware capture.
2. **Edit Signal**: Opened via a card's `✎` button to modify any property of an existing saved signal in-place.

Supported fields:

- signal name
- **Remote Control Device (RCD)**: Select an existing RCD from the dropdown or choose `+ New Remote Device...` to type a new device name
- protocol (`Raw` or `NEC`)
- frequency in kHz
- rawHex (optional, e.g. `0x00FF45BA`)
- comma-separated pulse values in microseconds (optional)
- **Shell command** (optional, e.g. `amixer set Master 5%+`, `playerctl play-pause`, or any custom script)

When saved:
- In Add mode, `AppController::addSignalManually()` creates and appends the signal to the specified RCD.
- In Edit mode, `AppController::updateSignal()` updates all properties in-place while preserving the signal's position and correctly managing RCD assignments.

### Capture via API
The capture flow starts a REST `POST /api/v1/capture` request. While the request is active, the UI shows a capture popup and waits for a remote button press. The underlying `ir_core` engine automatically suppresses ambient optical noise glitches and standalone NEC repeat frames, ensuring that only genuine command transmissions are captured.

When a valid signal arrives:

- the captured pulses are stored in memory and decoded if valid NEC
- **RCD Specification**: The user must specify the Remote Control Device (select an existing RCD from the dropdown or select `+ Create New Remote Device...` to input a new RCD name)
- the user specifies the signal name
- **Shell Command**: optional input field to configure a shell command associated with this signal
- clicking "Save Signal" commits the signal to the specified RCD in the local JSON storage

### Remove Saved Signal
Each saved-signal card includes a Delete (🗑) button. The action calls `SignalStorage::removeSignal()`:
- The signal is removed from its Remote Control Device.
- In accordance with the domain constraint ($1 \dots n$ signals per RCD), if the removed signal was the last signal in that RCD, the RCD is automatically deleted.
- The JSON file is rewritten immediately.

### Transmit Saved Signal
Each saved-signal card includes a Send (➤) button. This transmits the signal through the hardware API via `POST /api/v1/send`.

### Trigger Mode & Automation (`TriggerPopup.qml`)
Trigger Mode enables automated execution of shell commands when specific IR signals are received:
- Opened via the toolbar **`⚡ Trigger Mode`** button.
- **Armed Triggers**: Scans all saved signals in the local database and lists every command configured with a `shellCommand`. If no commands are configured, displays clear instructions on how to add or edit commands.
- **Execution Workflow**:
  1. User starts Trigger Mode by clicking **`▶ Start Trigger Mode`**.
  2. The application begins asynchronous, non-blocking polling through the HTTP REST API.
  3. When an armed IR signal is received, the app matches the signal in the local catalog and launches its assigned shell command asynchronously via the system shell (`/bin/sh -c` on Linux, `cmd.exe /c` on Windows).
  4. The event is timestamped and recorded in the real-time **Activity Log**.
  5. The app debounces repeat bursts and **immediately resumes listening for the next trigger command** in an ongoing cycle.
  6. Signals without shell commands or unrecognized noise are logged as ignored and do not interrupt the listening loop.
- **Background Operation**: The user can click "Run in Background" (or close the popup); Trigger Mode continues running with a live `Trigger Active (N)` indicator in the top bar. Click `⏹ Stop Trigger Mode` at any time to halt automated listening.

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

- `main.qml` — main window, signal card list with inline edit/transmit/delete buttons, RCD filter, action toolbar
- `TopBar.qml` — server and USB status indicators, live Trigger Mode badge
- `ManualAddPopup.qml` — dual-purpose modal for adding manual signals or editing all properties (including shell command) of existing signals
- `CapturePopup.qml` — capture waiting, RCD selector, signal naming, and optional shell command configuration UX
- `TriggerPopup.qml` — Trigger Mode modal with armed signals list, live execution activity log, and start/stop controls

### Controller Bridge

`AppController` acts as the bridge between QML and the C++ logic:

- reads the current API status from `/api/v1/status`
- exposes `rcdList` (`QStringList`) of existing device names
- exposes `selectedDeviceFilter` (`QString`) for filtering signals by device
- refreshes the signal grid from `SignalStorage`
- provides `getSignalDetails(index)` for pre-filling edit dialogues
- updates existing signals via `updateSignal(...)`
- saves captured signals (`saveCapturedSignal(...)`) and manual entries (`addSignalManually(...)`) with optional shell command parameters
- deletes signals and prunes empty RCDs
- transmits a saved signal through the hardware API
- manages **Trigger Mode** lifecycle: `startTriggerMode()`, `stopTriggerMode()`, `toggleTriggerMode()`, `clearTriggerLog()`
- exposes Trigger Mode reactive properties: `triggerModeActive` (`bool`), `armedTriggerCount` (`int`), `lastTriggeredSignal` (`QString`), `lastTriggeredCommand` (`QString`), `triggerStatusText` (`QString`), `triggerLog` (`QStringList`), `armedSignals` (`QVariantList`)

### Behaviour Contract

- hardware state is API-driven
- signal and RCD data is local-file driven
- REST API remains hardware-focused and does not expose signal CRUD endpoints
- GUI and TUI are synchronized because they both read and write the same storage file

---

## Notes

This module is intended to be the desktop application UI for local IR signal and device management. The API remains the transport layer for hardware actions, while the JSON file remains the single source of truth for saved Remote Control Devices and commands.
