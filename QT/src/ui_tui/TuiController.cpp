#include "TuiController.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <thread>
#include <sstream>
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <termios.h>
#endif

namespace ir {

static void flushTerminalInput() {
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    if (isatty(fileno(stdin))) {
        tcflush(fileno(stdin), TCIFLUSH);
    }
#endif
}

// Default Sharp Repro EQ button sequence from README.md
static const std::vector<int> DEFAULT_SHARP_PULSES = {
    6944, -4576, 544, -608, 544, -1728, 544, -608, 544, -608, 544, -608, 544,
    -608, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -1728, 592, -1680,
    592, -1680, 592, -1680, 592, -560, 592, -1680, 544, -1728, 544, -608, 544,
    -1728, 544, -1728, 544, -608, 544, -1728, 544, -1728, 544, -608, 544, -608,
    544, -1728, 544, -608, 544, -608, 544, -1728, 544, -608, 544, -608, 544,
    -1728, 544, -40688, 9008, -2288, 592, -304
};

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
    return std::string(buf);
}

TuiController::TuiController(std::shared_ptr<IrUsbTransceiver> transceiver)
    : m_transceiver(std::move(transceiver)), m_storage() {
    if (!m_transceiver) {
        m_transceiver = std::make_shared<IrUsbTransceiver>(38);
    }
    m_transceiver->setLogCallback([](const std::string& msg) {
        std::cout << "[USB] " << msg << "\n";
    });
}

void TuiController::printBanner() const {
    std::cout << "\n=======================================================\n";
    std::cout << "        Tiqiaa USB IR Transceiver Console Tool         \n";
    std::cout << "=======================================================\n";
}

void TuiController::printMenu() const {
    bool connected = m_transceiver && m_transceiver->isConnected();
    std::cout << "\n--- MAIN MENU --- [Device: "
              << (connected ? "CONNECTED" : "NOT CONNECTED") << "]\n";
    std::cout << "  1) Capture IR Signal (Single capture, 10s timeout)\n";
    std::cout << "  2) Continuous Monitor Mode (Real-time receive)\n";
    std::cout << "  3) Save Last Captured Signal to File\n";
    std::cout << "  4) List & Manage Saved Signals (" << m_storage.count() << " saved)\n";
    std::cout << "  5) Transmit (Send) IR Signal\n";
    std::cout << "  6) USB Hardware Information & Diagnostics\n";
    std::cout << "  7) Simulate IR Capture (Test without hardware)\n";
    std::cout << "  Q) Quit\n";
    std::cout << "Select option (1-7, Q): " << std::flush;
}

void TuiController::displaySignalDetails(const IrSignal& signal, const std::string& title, const std::string& identifiedName) {
    std::cout << "\n+------------------------------------------------------\n";
    std::cout << "| " << title << "\n";
    std::cout << "+------------------------------------------------------\n";
    if (!identifiedName.empty()) {
        std::cout << "| Signal Name: " << identifiedName << " [SAVED]\n";
    }
    std::cout << "| Pulse Count: " << signal.pulses.size() << "\n";
    std::cout << "| Frequency:   " << static_cast<int>(signal.frequency_khz) << " kHz\n";

    DecodedCode necCode;
    if (IrProtocol::decodeNec(signal.pulses, necCode)) {
        std::cout << "| Protocol:    NEC\n";
        std::cout << "| Address:     0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << necCode.address << std::dec << "\n";
        std::cout << "| Command:     0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << necCode.command << std::dec << "\n";
        std::cout << "| Raw Hex:     0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << necCode.rawValue << std::dec << "\n";
        std::cout << std::setfill(' ');
    } else {
        std::cout << "| Protocol:    Raw Pulse Sequence\n";
    }

    std::cout << "| Pulses [us]: " << signal.toString() << "\n";
    std::cout << "+------------------------------------------------------\n";
}

void TuiController::handleReceivedSignal(const IrSignal& sig, const std::string& title, bool promptIfNew) {
    m_lastCaptured = sig;
    auto matched = m_storage.findMatchingSignal(sig);
    if (matched) {
        displaySignalDetails(sig, title, matched->name);
        std::cout << "[IDENTIFIED] Signal already in library: \"" << matched->name
                  << "\" (" << matched->protocol << ")\n";
    } else {
        displaySignalDetails(sig, title, "");
        std::cout << "[INFO] Signal is not in saved library.\n";
        if (promptIfNew) {
            std::cout << "Enter signal name to save (press Enter for default name): " << std::flush;
            flushTerminalInput();
            std::string name;
            std::getline(std::cin, name);

            size_t first = name.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                size_t last = name.find_last_not_of(" \t\r\n");
                name = name.substr(first, last - first + 1);
            } else {
                name.clear();
            }

            if (name.empty()) {
                name = "IR_Signal_" + std::to_string(m_storage.count() + 1);
            }

            SavedIrSignal saved;
            saved.name = name;
            saved.frequency_khz = sig.frequency_khz;
            saved.pulses = sig.pulses;
            saved.timestamp = currentTimestamp();

            DecodedCode necCode;
            if (IrProtocol::decodeNec(saved.pulses, necCode)) {
                saved.protocol = "NEC";
                std::ostringstream ss;
                ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << necCode.rawValue;
                saved.rawHex = ss.str();
            } else {
                saved.protocol = "Raw";
                saved.rawHex = "N/A";
            }

            m_storage.addSignal(saved);
            std::cout << "[SUCCESS] Automatically saved new signal \"" << name
                      << "\" to " << m_storage.getStoragePath() << "!\n";
        }
    }
}

bool TuiController::captureOnce(float timeoutSeconds) {
    if (!m_transceiver->isConnected()) {
        std::cout << "[INFO] Hardware not connected. Attempting to open device...\n";
        if (!m_transceiver->openDevice()) {
            std::cout << "[ERROR] Could not connect to Tiqiaa USB device.\n";
            std::cout << "[TIP] Use option 7 to simulate a capture without hardware.\n";
            return false;
        }
    }

    std::cout << "\n>>> Point your remote at the transceiver and press a button...\n";
    std::cout << ">>> Listening (Timeout: " << timeoutSeconds << "s)...\n" << std::flush;

    IrSignal sig;
    if (m_transceiver->captureOnce(sig, timeoutSeconds)) {
        handleReceivedSignal(sig, "NEW IR SIGNAL CAPTURED", true);
        return true;
    } else {
        std::cout << "[TIMEOUT] No IR signal detected within " << timeoutSeconds << " seconds.\n";
        return false;
    }
}

void TuiController::startContinuousMonitor() {
    if (!m_transceiver->isConnected()) {
        if (!m_transceiver->openDevice()) {
            std::cout << "[ERROR] Could not open USB device. Ensure device is plugged in.\n";
            return;
        }
    }

    std::cout << "\n=======================================================\n";
    std::cout << " Continuous Monitor Mode Active\n";
    std::cout << " Point remote and press buttons. Press ENTER to return to menu.\n";
    std::cout << "=======================================================\n";

    m_transceiver->setSignalReceivedCallback([this](const IrSignal& sig) {
        handleReceivedSignal(sig, "SIGNAL CAPTURED (CONTINUOUS MONITOR)", false);
    });

    m_transceiver->startBackgroundPolling();

    // Wait for user to hit enter
    std::string dummy;
    std::getline(std::cin, dummy);

    m_transceiver->stopBackgroundPolling();
    m_transceiver->setSignalReceivedCallback(nullptr);
    std::cout << "[INFO] Continuous monitor stopped.\n";
}

void TuiController::saveLastCapturedSignal() {
    if (!m_lastCaptured) {
        std::cout << "[WARNING] No signal captured yet! Capture a signal first (Option 1 or 7).\n";
        return;
    }

    std::cout << "\nSave Signal:\n";
    std::cout << "Enter a friendly name (e.g., 'TV Power', 'Audio Volume Up'): " << std::flush;
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        name = "IR_Signal_" + std::to_string(m_storage.count() + 1);
    }

    SavedIrSignal saved;
    saved.name = name;
    saved.frequency_khz = m_lastCaptured->frequency_khz;
    saved.pulses = m_lastCaptured->pulses;
    saved.timestamp = currentTimestamp();

    DecodedCode necCode;
    if (IrProtocol::decodeNec(saved.pulses, necCode)) {
        saved.protocol = "NEC";
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << necCode.rawValue;
        saved.rawHex = ss.str();
    } else {
        saved.protocol = "Raw";
        saved.rawHex = "N/A";
    }

    m_storage.addSignal(saved);
    std::cout << "[SUCCESS] Saved '" << name << "' to " << m_storage.getStoragePath() << "!\n";
}

void TuiController::listSavedSignals() {
    size_t count = m_storage.count();
    if (count == 0) {
        std::cout << "\n[INFO] No signals currently saved in " << m_storage.getStoragePath() << ".\n";
        return;
    }

    std::cout << "\n================ SAVED SIGNALS (" << count << ") ================\n";
    std::cout << "Storage location: " << m_storage.getStoragePath() << "\n";
    std::cout << std::setfill(' ') << std::left << std::setw(4) << "#"
              << std::setw(24) << "Name"
              << std::setw(10) << "Protocol"
              << std::setw(10) << "Freq(kHz)"
              << std::setw(8) << "Pulses"
              << std::setw(20) << "Timestamp"
              << "\n---------------------------------------------------------------\n";

    const auto& list = m_storage.getSignals();
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << std::left << std::setw(4) << (i + 1)
                  << std::setw(24) << list[i].name
                  << std::setw(10) << list[i].protocol
                  << std::setw(10) << static_cast<int>(list[i].frequency_khz)
                  << std::setw(8) << list[i].pulses.size()
                  << std::setw(20) << list[i].timestamp
                  << "\n";
    }
    std::cout << "===============================================================\n";
    std::cout << "Actions: (D # to delete, Enter to return): " << std::flush;
    std::string act;
    std::getline(std::cin, act);
    if (!act.empty() && (act[0] == 'd' || act[0] == 'D')) {
        std::istringstream iss(act.substr(1));
        size_t idx;
        if (iss >> idx && idx >= 1 && idx <= list.size()) {
            std::string delName = list[idx - 1].name;
            m_storage.removeSignal(idx - 1);
            std::cout << "[INFO] Removed '" << delName << "'.\n";
        }
    }
}

void TuiController::transmitSavedSignal() {
    size_t count = m_storage.count();
    std::cout << "\nTransmit Options:\n";
    std::cout << "  0) Default Sharp Repro EQ Preset (" << DEFAULT_SHARP_PULSES.size() << " pulses)\n";
    if (count > 0) {
        const auto& list = m_storage.getSignals();
        for (size_t i = 0; i < list.size(); ++i) {
            std::cout << "  " << (i + 1) << ") " << list[i].name
                      << " (" << list[i].protocol << ", " << list[i].pulses.size() << " pulses)\n";
        }
    } else {
        std::cout << "  (No saved signals found in " << m_storage.getStoragePath() << ")\n";
    }
    std::cout << "Select signal number or name to transmit (or 'M' for manual pulses): " << std::flush;

    std::string choice;
    std::getline(std::cin, choice);
    if (choice.empty()) return;

    size_t first = choice.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    size_t last = choice.find_last_not_of(" \t\r\n");
    choice = choice.substr(first, last - first + 1);

    if (choice == "M" || choice == "m") {
        transmitManualSequence();
        return;
    }

    std::vector<int> pulsesToSend;
    uint8_t freq = 38;
    std::string name;
    bool found = false;

    try {
        int idx = std::stoi(choice);
        if (idx == 0) {
            pulsesToSend = DEFAULT_SHARP_PULSES;
            name = "Sharp Repro EQ Preset";
            found = true;
        } else if (idx > 0 && static_cast<size_t>(idx) <= count) {
            const auto& sig = m_storage.getSignals()[idx - 1];
            pulsesToSend = sig.pulses;
            freq = sig.frequency_khz;
            name = sig.name;
            found = true;
        }
    } catch (...) {
        // Not a number, check by name below
    }

    if (!found && count > 0) {
        const auto& list = m_storage.getSignals();
        for (const auto& sig : list) {
            if (sig.name == choice) {
                pulsesToSend = sig.pulses;
                freq = sig.frequency_khz;
                name = sig.name;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        std::cout << "[ERROR] Invalid selection: '" << choice << "'.\n";
        return;
    }

    if (!m_transceiver->isConnected()) {
        if (!m_transceiver->openDevice()) {
            std::cout << "[ERROR] Cannot transmit: Hardware device not found.\n";
            return;
        }
    }

    std::cout << "[INFO] Transmitting '" << name << "' (" << pulsesToSend.size() << " pulses)...\n";
    if (m_transceiver->transmitIr(pulsesToSend, freq)) {
        std::cout << "[SUCCESS] Transmission complete!\n";
    } else {
        std::cout << "[ERROR] Transmission failed.\n";
    }
}

void TuiController::transmitManualSequence() {
    std::cout << "\nEnter pulse sequence (e.g. 9000, -4500, 560, -560...): " << std::flush;
    std::string input;
    std::getline(std::cin, input);
    auto pulses = IrProtocol::parsePulseSequence(input);
    if (pulses.empty()) {
        std::cout << "[ERROR] Empty or invalid pulse sequence.\n";
        return;
    }

    if (!m_transceiver->isConnected()) {
        if (!m_transceiver->openDevice()) {
            std::cout << "[ERROR] Cannot transmit: Hardware device not found.\n";
            return;
        }
    }

    std::cout << "[INFO] Transmitting " << pulses.size() << " pulses...\n";
    if (m_transceiver->transmitIr(pulses, 38)) {
        std::cout << "[SUCCESS] Transmission complete!\n";
    } else {
        std::cout << "[ERROR] Transmission failed.\n";
    }
}

void TuiController::printHardwareInfo() {
    std::cout << "\n================ USB HARDWARE STATUS ================\n";
    if (!m_transceiver->isConnected()) {
        std::cout << "Device Status: Disconnected\n";
        std::cout << "Attempting connection...\n";
        m_transceiver->openDevice();
    }

    DeviceInfo info = m_transceiver->getDeviceInfo();
    std::cout << "Connected:         " << (info.isConnected ? "YES" : "NO") << "\n";
    std::cout << "Manufacturer:      " << info.manufacturer << "\n";
    std::cout << "Product:           " << info.product << "\n";
    std::cout << "Serial Number:     " << info.serialNumber << "\n";
    std::cout << "Vendor ID (VID):   0x" << std::hex << std::uppercase << info.vendorId << std::dec << "\n";
    std::cout << "Product ID (PID):  0x" << std::hex << std::uppercase << info.productId << std::dec << "\n";
    std::cout << "Default Frequency: " << static_cast<int>(info.defaultFrequency) << " kHz\n";

    if (info.isConnected) {
        std::string fw = m_transceiver->getFirmwareVersion();
        std::cout << "Firmware Version:  " << fw << "\n";
    }
    std::cout << "=====================================================\n";
}

void TuiController::simulateCapture() {
    std::cout << "\n[SIMULATOR] Simulation Presets:\n";
    std::cout << "  1) Sample: Volume Up (Addr 0x00, Cmd 0x18)\n";
    std::cout << "  2) Sample: Ceiling Fan (Addr 0x01, Cmd 0x5A)\n";
    std::cout << "  3) Custom NEC Code (specify Address & Command)\n";
    std::cout << "Select simulation preset (1-3, default 1): " << std::flush;

    std::string choice;
    std::getline(std::cin, choice);

    uint32_t addr = 0x00;
    uint32_t cmd = 0x18;

    if (choice == "2") {
        addr = 0x01;
        cmd = 0x5A;
    } else if (choice == "3") {
        std::cout << "Enter Address in hex or dec (e.g. 0x02 or 2): " << std::flush;
        std::string addrStr;
        std::getline(std::cin, addrStr);
        std::cout << "Enter Command in hex or dec (e.g. 0x20 or 32): " << std::flush;
        std::string cmdStr;
        std::getline(std::cin, cmdStr);
        try {
            addr = std::stoul(addrStr, nullptr, 0);
            cmd = std::stoul(cmdStr, nullptr, 0);
        } catch (...) {
            std::cout << "[WARN] Invalid input, defaulting to Addr 0x02, Cmd 0x20.\n";
            addr = 0x02;
            cmd = 0x20;
        }
    }

    std::cout << "\n[SIMULATOR] Generating NEC IR Signal (Addr 0x"
              << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << addr
              << ", Cmd 0x" << std::setw(2) << std::setfill('0') << cmd
              << std::dec << ")...\n";

    std::vector<int> necPulses = IrProtocol::encodeNec(addr, cmd);

    IrSignal simSig;
    simSig.pulses = necPulses;
    simSig.frequency_khz = 38;
    simSig.timestamp = std::chrono::system_clock::now();

    handleReceivedSignal(simSig, "SIMULATED IR CODE GENERATED", true);
}

void TuiController::runInteractiveLoop() {
    printBanner();

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    if (!isatty(fileno(stdin))) {
        std::cout << "\n=======================================================\n";
        std::cout << "[NOTICE] Running without an interactive terminal.\n";
        std::cout << "To interact with this menu in Qt Creator:\n";
        std::cout << "  1. Click 'Projects' (left sidebar, Ctrl+5)\n";
        std::cout << "  2. Under your Kit, click 'Run'\n";
        std::cout << "  3. Check the box [x] 'Run in terminal'\n";
        std::cout << "=======================================================\n\n";
    }
#endif

    // Auto-connect if possible
    m_transceiver->openDevice();

    bool running = true;
    while (running && std::cin.good()) {
        printMenu();
        std::string choice;
        if (!std::getline(std::cin, choice)) {
            // Reached EOF or input stream closed
            break;
        }

        // Trim leading and trailing whitespace
        size_t first = choice.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }
        size_t last = choice.find_last_not_of(" \t\r\n");
        choice = choice.substr(first, last - first + 1);

        // Ignore lines that look like echoes of headers / menu banners
        if (choice.empty() || choice[0] == '=' || choice[0] == '-' || choice.find("MAIN MENU") != std::string::npos) {
            continue;
        }

        // A valid menu option is ONLY 1 character ('1'-'7' or 'Q').
        // If the line is longer, it is echoed output or invalid input — ignore it!
        if (choice.size() > 2) {
            continue;
        }

        char opt = std::toupper(choice[0]);
        switch (opt) {
            case '1':
                captureOnce();
                break;
            case '2':
                startContinuousMonitor();
                break;
            case '3':
                saveLastCapturedSignal();
                break;
            case '4':
                listSavedSignals();
                break;
            case '5':
                transmitSavedSignal();
                break;
            case '6':
                printHardwareInfo();
                break;
            case '7':
                simulateCapture();
                break;
            case 'Q':
                std::cout << "Exiting console test tool. Goodbye!\n";
                running = false;
                break;
            default:
                std::cout << "[ERROR] Unknown option: '" << choice << "'. Please select 1-7 or Q.\n";
                break;
        }
    }
}

} // namespace ir
