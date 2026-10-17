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
#include <poll.h>
#elif defined(_WIN32)
#include <conio.h>
#endif

namespace ir {

static void flushTerminalInput() {
#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    if (isatty(fileno(stdin))) {
        tcflush(fileno(stdin), TCIFLUSH);
    }
#endif
}

static bool checkStdinHasInput() {
#if defined(_WIN32)
    return _kbhit() != 0;
#elif defined(__unix__) || defined(__linux__) || defined(__APPLE__)
    struct pollfd pfd;
    pfd.fd = fileno(stdin);
    pfd.events = POLLIN;
    pfd.revents = 0;
    return poll(&pfd, 1, 0) > 0;
#else
    return false;
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

TuiController::TuiController(std::string apiBaseUrl)
    : m_storage() {
    setApiBaseUrl(apiBaseUrl);
}

TuiController::TuiController(std::string host, int port)
    : m_host(std::move(host)), m_port(port), m_storage() {
    m_apiBaseUrl = "http://" + m_host + ":" + std::to_string(m_port);
}

void TuiController::setApiBaseUrl(const std::string& url) {
    m_apiBaseUrl = url;
    if (m_apiBaseUrl.find("http://") != 0 && m_apiBaseUrl.find("https://") != 0) {
        m_apiBaseUrl = "http://" + m_apiBaseUrl;
    }
    while (m_apiBaseUrl.size() > 1 && m_apiBaseUrl.back() == '/') {
        m_apiBaseUrl.pop_back();
    }

    // Parse host and port
    std::string noProto = m_apiBaseUrl.substr(m_apiBaseUrl.find("://") + 3);
    auto colonPos = noProto.find(':');
    auto slashPos = noProto.find('/');
    if (colonPos != std::string::npos) {
        m_host = noProto.substr(0, colonPos);
        std::string portStr = (slashPos != std::string::npos)
            ? noProto.substr(colonPos + 1, slashPos - colonPos - 1)
            : noProto.substr(colonPos + 1);
        try {
            m_port = std::stoi(portStr);
        } catch (...) {
            m_port = 8080;
        }
    } else {
        m_host = (slashPos != std::string::npos) ? noProto.substr(0, slashPos) : noProto;
        m_port = (m_apiBaseUrl.rfind("https://", 0) == 0) ? 443 : 80;
    }
}

std::unique_ptr<httplib::Client> TuiController::createClient(int timeoutSeconds) const {
    auto client = std::make_unique<httplib::Client>(m_apiBaseUrl);
    client->set_connection_timeout(timeoutSeconds);
    client->set_read_timeout(timeoutSeconds);
    client->set_write_timeout(timeoutSeconds);
    return client;
}

bool TuiController::isApiReachable() {
    auto client = createClient(2);
    if (!client) return false;
    auto res = client->Get("/api/v1/status");
    return res && (res->status == 200 || res->status == 503);
}

bool TuiController::isDeviceConnected() {
    auto client = createClient(2);
    if (!client) return false;
    auto res = client->Get("/api/v1/status");
    if (!res || res->status != 200) return false;
    try {
        auto j = nlohmann::json::parse(res->body);
        return j.value("connected", false);
    } catch (...) {
        return false;
    }
}

void TuiController::printBanner() const {
    std::cout << "\n=======================================================\n";
    std::cout << "        Tiqiaa USB IR Transceiver Console Tool         \n";
    std::cout << "             (Connected via HTTP REST API)             \n";
    std::cout << "=======================================================\n";
}

void TuiController::printMenu() {
    std::string devStatus = "SERVER OFFLINE";
    auto client = createClient(2);
    if (client) {
        auto res = client->Get("/api/v1/status");
        if (res && res->status == 200) {
            try {
                auto j = nlohmann::json::parse(res->body);
                if (j.value("connected", false)) {
                    devStatus = "CONNECTED";
                } else {
                    devStatus = "DISCONNECTED";
                }
            } catch (...) {
                devStatus = "ONLINE";
            }
        }
    }

    std::cout << "\n--- MAIN MENU --- [API: " << m_apiBaseUrl
              << " | Device: " << devStatus << "]\n";
    std::cout << "  1) Capture IR Signal (Single capture via HTTP API)\n";
    std::cout << "  2) Continuous Monitor Mode (Live polling stream)\n";
    std::cout << "  3) Save Last Captured Signal to File & API Server\n";
    std::cout << "  4) List & Manage Saved Signals (" << m_storage.count() << " saved)\n";
    std::cout << "  5) Transmit (Send) IR Signal via HTTP API\n";
    std::cout << "  6) USB Hardware Information & Diagnostics (via API)\n";
    std::cout << "  7) Simulate IR Capture (Test without hardware)\n";
    std::cout << "  Q) Quit\n";
    std::cout << "Select option (1-7, Q): " << std::flush;
}

void TuiController::displaySignalDetails(const IrSignal& signal, const std::string& title, const std::string& identifiedName, const std::string& identifiedRcd) {
    std::cout << "\n+------------------------------------------------------\n";
    std::cout << "| " << title << "\n";
    std::cout << "+------------------------------------------------------\n";
    if (!identifiedRcd.empty()) {
        std::cout << "| Remote Device: " << identifiedRcd << "\n";
    }
    if (!identifiedName.empty()) {
        std::cout << "| Signal Name:   " << identifiedName << " [SAVED]\n";
    }
    std::cout << "| Pulse Count:   " << signal.pulses.size() << "\n";
    std::cout << "| Frequency:     " << static_cast<int>(signal.frequency_khz) << " kHz\n";

    DecodedCode necCode;
    if (IrProtocol::decodeNec(signal.pulses, necCode)) {
        std::cout << "| Protocol:      NEC" << (necCode.address > 0xFF ? " (Extended 16-bit)" : "") << "\n";
        std::cout << "| Address:       0x" << std::hex << std::uppercase << (necCode.address > 0xFF ? std::setw(4) : std::setw(2)) << std::setfill('0') << necCode.address << std::dec << "\n";
        std::cout << "| Command:       0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << necCode.command << std::dec << "\n";
        std::cout << "| Raw Hex:       0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << necCode.rawValue << std::dec << "\n";
        std::cout << std::setfill(' ');
    } else {
        std::cout << "| Protocol:      Raw Pulse Sequence\n";
    }

    std::cout << "| Pulses [us]:   " << signal.toString() << "\n";
    std::cout << "+------------------------------------------------------\n";
}

std::string TuiController::promptForRcd() {
    auto deviceNames = m_storage.getDeviceNames();
    std::cout << "\nSpecify Remote Control Device (RCD):\n";
    if (!deviceNames.empty()) {
        for (size_t i = 0; i < deviceNames.size(); ++i) {
            std::cout << "  " << (i + 1) << ") [Existing] " << deviceNames[i]
                      << " (" << m_storage.count(deviceNames[i]) << " signals)\n";
        }
        std::cout << "Select existing RCD (1-" << deviceNames.size() << ") or enter new RCD name: " << std::flush;
    } else {
        std::cout << "Enter RCD name [Default Remote Device]: " << std::flush;
    }
    flushTerminalInput();
    std::string input;
    std::getline(std::cin, input);
    size_t first = input.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
        size_t last = input.find_last_not_of(" \t\r\n");
        input = input.substr(first, last - first + 1);
    } else {
        input.clear();
    }

    if (input.empty()) {
        return deviceNames.empty() ? "Default Remote Device" : deviceNames[0];
    }

    try {
        size_t idx = std::stoul(input);
        if (idx >= 1 && idx <= deviceNames.size()) {
            return deviceNames[idx - 1];
        }
    } catch (...) {}

    return input;
}

void TuiController::handleReceivedSignal(const IrSignal& sig, const std::string& title, bool promptIfNew) {
    m_lastCaptured = sig;
    auto matched = m_storage.findMatchingSignal(sig);
    if (matched) {
        displaySignalDetails(sig, title, matched->name, matched->rcdName);
        std::cout << "[IDENTIFIED] Signal already in library: \"" << matched->name
                  << "\" on device \"" << matched->rcdName << "\" (" << matched->protocol << ")\n";
    } else {
        displaySignalDetails(sig, title, "");
        std::cout << "[INFO] Signal is not in saved library.\n";
        if (promptIfNew) {
            std::cout << "Enter signal name to save (press Enter to skip): " << std::flush;
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
                std::cout << "[INFO] Signal not saved.\n";
                return;
            }

            std::string rcdName = promptForRcd();

            SavedIrSignal saved;
            saved.name = name;
            saved.rcdName = rcdName;
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

            m_storage.addSignal(saved, rcdName);
            std::cout << "[SUCCESS] Saved new signal \"" << name
                      << "\" in RCD \"" << rcdName << "\" to local storage (" << m_storage.getStoragePath() << ")!\n";
        }
    }
}

bool TuiController::captureOnce(float timeoutSeconds) {
    if (!isDeviceConnected()) {
        std::cerr << "[ERROR] Cannot capture: IR Transceiver hardware is not connected.\n";
        return false;
    }

    auto client = createClient(static_cast<int>(timeoutSeconds) + 5);
    if (!client) {
        std::cerr << "[ERROR] Could not initialize HTTP client.\n";
        return false;
    }

    std::cout << "\n>>> Point your remote at the transceiver and press a button...\n";
    std::cout << ">>> Listening via HTTP API at " << m_apiBaseUrl << " (Timeout: " << timeoutSeconds << "s)...\n" << std::flush;

    nlohmann::json reqBody = {{"timeout", timeoutSeconds}};
    auto res = client->Post("/api/v1/capture", reqBody.dump(), "application/json");

    if (!res) {
        std::cerr << "[ERROR] Failed to communicate with API server at " << m_apiBaseUrl << ".\n";
        std::cerr << "[TIP] Ensure ir_api_server is running and reachable.\n";
        return false;
    }

    if (res->status == 200) {
        try {
            auto j = nlohmann::json::parse(res->body);
            if (j.contains("signal") && j["signal"].contains("pulses")) {
                IrSignal sig;
                sig.pulses = j["signal"]["pulses"].get<std::vector<int>>();
                sig.frequency_khz = static_cast<uint8_t>(j["signal"].value("frequency", 38));
                sig.timestamp = std::chrono::system_clock::now();
                handleReceivedSignal(sig, "NEW IR SIGNAL CAPTURED", true);
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to parse capture response: " << e.what() << "\n";
            return false;
        }
    } else if (res->status == 408) {
        std::cout << "[TIMEOUT] No IR signal detected within " << timeoutSeconds << " seconds.\n";
        return false;
    } else {
        std::string err = res->body;
        try {
            auto j = nlohmann::json::parse(res->body);
            err = j.value("error", err);
        } catch (...) {}
        std::cerr << "[ERROR] Capture request failed (HTTP " << res->status << "): " << err << "\n";
        return false;
    }
    return false;
}

void TuiController::startContinuousMonitor() {
    if (!isDeviceConnected()) {
        std::cerr << "[ERROR] Cannot start continuous monitor: IR Transceiver hardware is not connected.\n";
        return;
    }

    auto client = createClient(5);
    if (!client) {
        std::cerr << "[ERROR] Could not connect to API server at " << m_apiBaseUrl << ".\n";
        return;
    }

    std::cout << "\n=======================================================\n";
    std::cout << " Continuous Monitor Mode Active (via HTTP API)\n";
    std::cout << " Point remote and press buttons. Press ENTER to return to menu.\n";
    std::cout << "=======================================================\n";

    m_monitoring = true;
    while (m_monitoring.load()) {
        if (checkStdinHasInput()) {
            std::string dummy;
            std::getline(std::cin, dummy);
            break;
        }

        auto pollClient = createClient(4);
        if (!pollClient) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        nlohmann::json reqBody = {{"timeout", 1.5f}};
        auto res = pollClient->Post("/api/v1/capture", reqBody.dump(), "application/json");

        if (checkStdinHasInput()) {
            std::string dummy;
            std::getline(std::cin, dummy);
            break;
        }

        if (res && res->status == 200) {
            try {
                auto j = nlohmann::json::parse(res->body);
                if (j.contains("signal") && j["signal"].contains("pulses")) {
                    IrSignal sig;
                    sig.pulses = j["signal"]["pulses"].get<std::vector<int>>();
                    sig.frequency_khz = static_cast<uint8_t>(j["signal"].value("frequency", 38));
                    sig.timestamp = std::chrono::system_clock::now();
                    handleReceivedSignal(sig, "SIGNAL CAPTURED (CONTINUOUS MONITOR)", true);
                    std::cout << "\n>>> Continuing monitor... (Press ENTER to return to menu)\n" << std::flush;
                }
            } catch (...) {}
        } else if (res && res->status == 503) {
            std::cerr << "[ERROR] Transceiver disconnected. Stopping continuous monitor.\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    m_monitoring = false;
    std::cout << "[INFO] Continuous monitor stopped.\n";
}

void TuiController::saveLastCapturedSignal() {
    if (!m_lastCaptured) {
        std::cout << "[WARNING] No signal captured yet! Capture a signal first (Option 1, 2, or 7).\n";
        return;
    }

    std::cout << "\nSave Signal:\n";
    std::cout << "Enter a friendly name (press Enter to cancel): " << std::flush;
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
        std::cout << "[INFO] Signal not saved.\n";
        return;
    }

    std::string rcdName = promptForRcd();

    SavedIrSignal saved;
    saved.name = name;
    saved.rcdName = rcdName;
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

    m_storage.addSignal(saved, rcdName);
    std::cout << "[SUCCESS] Saved '" << name << "' to RCD '" << rcdName << "' in local storage (" << m_storage.getStoragePath() << ")!\n";
}

void TuiController::listSavedSignals() {
    size_t count = m_storage.count();
    if (count == 0) {
        std::cout << "\n[INFO] No signals currently saved in local storage (" << m_storage.getStoragePath() << ").\n";
    } else {
        std::cout << "\n================ SAVED SIGNALS (" << count << ") ================\n";
        std::cout << "Storage location: " << m_storage.getStoragePath() << "\n";
        std::cout << "Remote Control Devices (" << m_storage.deviceCount() << "):\n";

        size_t globalIdx = 1;
        for (const auto& dev : m_storage.getDevices()) {
            std::cout << "\n  [" << dev.name << " (" << dev.savedSignals.size() << " signal" << (dev.savedSignals.size() == 1 ? "" : "s") << ")]\n";
            std::cout << std::setfill(' ') << std::left << std::setw(4) << "    #"
                      << std::setw(24) << "Name"
                      << std::setw(10) << "Protocol"
                      << std::setw(10) << "Freq(kHz)"
                      << std::setw(8) << "Pulses"
                      << std::setw(20) << "Timestamp"
                      << "\n    -----------------------------------------------------------\n";

            for (const auto& sig : dev.savedSignals) {
                std::cout << "    " << std::left << std::setw(4) << globalIdx++
                          << std::setw(24) << sig.name
                          << std::setw(10) << sig.protocol
                          << std::setw(10) << static_cast<int>(sig.frequency_khz)
                          << std::setw(8) << sig.pulses.size()
                          << std::setw(20) << sig.timestamp
                          << "\n";
            }
        }
        std::cout << "===============================================================\n";
    }

    std::cout << "Actions: (D # to delete, Enter to return): " << std::flush;
    std::string act;
    std::getline(std::cin, act);
    if (!act.empty() && (act[0] == 'd' || act[0] == 'D')) {
        std::istringstream iss(act.substr(1));
        size_t idx;
        if (iss >> idx && idx >= 1 && idx <= m_storage.count()) {
            auto sigOpt = m_storage.getSignal(idx - 1);
            std::string delName = sigOpt ? sigOpt->name : "Signal";
            std::string delRcd = sigOpt ? sigOpt->rcdName : "";
            m_storage.removeSignal(idx - 1);
            std::cout << "[INFO] Removed '" << delName << "' (" << delRcd << ") from local storage.\n";
        }
    }
}

void TuiController::transmitSavedSignal() {
    size_t count = m_storage.count();
    std::cout << "\nTransmit Options:\n";
    std::cout << "  0) Default Sharp Repro EQ Preset (" << DEFAULT_SHARP_PULSES.size() << " pulses)\n";
    if (count > 0) {
        size_t globalIdx = 1;
        for (const auto& dev : m_storage.getDevices()) {
            std::cout << "\n  [" << dev.name << "]\n";
            for (const auto& sig : dev.savedSignals) {
                std::cout << "    " << globalIdx++ << ") " << sig.name
                          << " (" << sig.protocol << ", " << sig.pulses.size() << " pulses)\n";
            }
        }
    } else {
        std::cout << "  (No saved signals found in local storage: " << m_storage.getStoragePath() << ")\n";
    }
    std::cout << "\nSelect signal number or name to transmit (or 'M' for manual pulses): " << std::flush;

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

    transmitPulses(pulsesToSend, freq, name);
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

    transmitPulses(pulses, 38, "Manual Pulse Sequence");
}

bool TuiController::transmitPulses(const std::vector<int>& pulses, uint8_t frequency_khz, const std::string& name) {
    if (pulses.empty()) {
        std::cerr << "[ERROR] Cannot transmit empty pulse sequence.\n";
        return false;
    }

    auto client = createClient(5);
    if (!client) {
        std::cerr << "[ERROR] Could not initialize HTTP client.\n";
        return false;
    }

    nlohmann::json reqBody = {
        {"protocol", "Raw"},
        {"pulses", pulses},
        {"frequency", static_cast<int>(frequency_khz)}
    };

    std::cout << "[INFO] Transmitting '" << name << "' (" << pulses.size() << " pulses, "
              << static_cast<int>(frequency_khz) << " kHz) via API...\n";

    auto res = client->Post("/api/v1/send", reqBody.dump(), "application/json");
    if (!res) {
        std::cerr << "[ERROR] Failed to send transmission request to API server at " << m_apiBaseUrl << ".\n";
        return false;
    }

    if (res->status == 200) {
        std::cout << "[SUCCESS] Transmission of '" << name << "' complete!\n";
        return true;
    } else {
        std::string err = res->body;
        try {
            auto j = nlohmann::json::parse(res->body);
            err = j.value("error", err);
        } catch (...) {}
        std::cerr << "[ERROR] Transmission failed (HTTP " << res->status << "): " << err << "\n";
        return false;
    }
}

void TuiController::printHardwareInfo() {
    std::cout << "\n================ HARDWARE & API STATUS ================\n";
    std::cout << "API Server URL:    " << m_apiBaseUrl << "\n";

    auto client = createClient(3);
    if (!client) {
        std::cout << "API Status:        ERROR (Client creation failed)\n";
        std::cout << "=======================================================\n";
        return;
    }

    auto res = client->Get("/api/v1/status");
    if (!res) {
        std::cout << "API Status:        OFFLINE (Could not connect to " << m_apiBaseUrl << ")\n";
        std::cout << "=======================================================\n";
        return;
    }

    std::cout << "API Status:        ONLINE (HTTP " << res->status << ")\n";
    try {
        auto j = nlohmann::json::parse(res->body);
        bool connected = j.value("connected", false);
        std::string state = j.value("state", "Unknown");
        std::cout << "Device Connected:  " << (connected ? "YES" : "NO") << "\n";
        std::cout << "Device State:      " << state << "\n";

        if (j.contains("device") && j["device"].is_object()) {
            const auto& dev = j["device"];
            std::cout << "Manufacturer:      " << dev.value("manufacturer", "N/A") << "\n";
            std::cout << "Product:           " << dev.value("product", "N/A") << "\n";
            std::cout << "Serial Number:     " << dev.value("serialNumber", "N/A") << "\n";
            std::cout << "Vendor ID (VID):   " << dev.value("vendorId", "N/A") << "\n";
            std::cout << "Product ID (PID):  " << dev.value("productId", "N/A") << "\n";
            std::cout << "Default Frequency: " << dev.value("defaultFrequency", 38) << " kHz\n";
            std::cout << "Firmware Version:  " << dev.value("firmwareVersion", "N/A") << "\n";
        }
        if (j.contains("savedSignalsCount")) {
            std::cout << "Server Signals:    " << j["savedSignalsCount"] << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Failed to parse API status response: " << e.what() << "\n";
    }
    std::cout << "=======================================================\n";
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

    if (!isApiReachable()) {
        std::cout << "[WARNING] Cannot reach HTTP API server at " << m_apiBaseUrl << ".\n";
        std::cout << "Please ensure ir_api_server is running (e.g. ./build/debug/ir_api_server).\n\n";
    }

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
