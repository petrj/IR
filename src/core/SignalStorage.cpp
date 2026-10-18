#include "SignalStorage.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>

namespace ir {

static std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string SavedIrSignal::toJsonString() const {
    nlohmann::json j;
    j["name"] = name;
    j["protocol"] = protocol;
    j["frequency"] = static_cast<int>(frequency_khz);
    j["timestamp"] = timestamp;
    j["rawHex"] = rawHex;
    j["pulses"] = pulses;
    if (!rcdName.empty()) {
        j["rcdName"] = rcdName;
    }
    if (!shellCommand.empty()) {
        j["shellCommand"] = shellCommand;
    }
    return j.dump(2);
}

std::string SignalStorage::getDefaultStoragePath() {
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && xdgConfigHome[0] != '\0') {
        std::filesystem::path configPath(xdgConfigHome);
        configPath /= "IR";
        return (configPath / "saved_ir_signals.json").string();
    }

    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        std::filesystem::path configPath(home);
        configPath /= ".config";
        configPath /= "IR";
        return (configPath / "saved_ir_signals.json").string();
    }

#if defined(_WIN32)
    const char* appData = std::getenv("APPDATA");
    if (appData && appData[0] != '\0') {
        std::filesystem::path configPath(appData);
        configPath /= "IR";
        return (configPath / "saved_ir_signals.json").string();
    }

    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && localAppData[0] != '\0') {
        std::filesystem::path configPath(localAppData);
        configPath /= "IR";
        return (configPath / "saved_ir_signals.json").string();
    }

    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && userProfile[0] != '\0') {
        std::filesystem::path configPath(userProfile);
        configPath /= "AppData";
        configPath /= "Roaming";
        configPath /= "IR";
        return (configPath / "saved_ir_signals.json").string();
    }
#endif

    return "saved_ir_signals.json";
}

SignalStorage::SignalStorage(std::string storagePath)
    : m_storagePath(std::move(storagePath)) {
    bool isCustomPath = !m_storagePath.empty();
    if (m_storagePath.empty()) {
        m_storagePath = getDefaultStoragePath();
    }

    std::filesystem::path storageFilePath(m_storagePath);
    if (storageFilePath.has_parent_path()) {
        std::filesystem::create_directories(storageFilePath.parent_path());
    }

    if (!loadFromFile()) {
        if (!isCustomPath && m_storagePath != "saved_ir_signals.json") {
            if (loadFromFile("saved_ir_signals.json")) {
                saveToFile();
            }
        }
    }
}

void SignalStorage::rebuildCache() const {
    m_cachedSignals.clear();
    for (const auto& dev : m_devices) {
        for (auto sig : dev.savedSignals) {
            sig.rcdName = dev.name;
            m_cachedSignals.push_back(std::move(sig));
        }
    }
    m_cacheDirty = false;
}

const std::string& SignalStorage::getStoragePath() const {
    return m_storagePath;
}

const std::vector<RemoteControlDevice>& SignalStorage::getDevices() const {
    return m_devices;
}

std::vector<RemoteControlDevice>& SignalStorage::getDevices() {
    m_cacheDirty = true;
    return m_devices;
}

std::vector<std::string> SignalStorage::getDeviceNames() const {
    std::vector<std::string> names;
    names.reserve(m_devices.size());
    for (const auto& dev : m_devices) {
        names.push_back(dev.name);
    }
    return names;
}

bool SignalStorage::hasDevice(const std::string& name) const {
    std::string trimmed = trimString(name);
    for (const auto& dev : m_devices) {
        if (dev.name == trimmed) return true;
    }
    return false;
}

const RemoteControlDevice* SignalStorage::getDevice(const std::string& name) const {
    std::string trimmed = trimString(name);
    for (const auto& dev : m_devices) {
        if (dev.name == trimmed) return &dev;
    }
    return nullptr;
}

RemoteControlDevice* SignalStorage::getDevice(const std::string& name) {
    std::string trimmed = trimString(name);
    for (auto& dev : m_devices) {
        if (dev.name == trimmed) return &dev;
    }
    return nullptr;
}

bool SignalStorage::addDevice(const std::string& name) {
    std::string trimmed = trimString(name);
    if (trimmed.empty() || hasDevice(trimmed)) {
        return false;
    }
    m_devices.push_back(RemoteControlDevice{trimmed, {}});
    m_cacheDirty = true;
    return true;
}

bool SignalStorage::removeDevice(const std::string& name) {
    std::string trimmed = trimString(name);
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->name == trimmed) {
            m_devices.erase(it);
            m_cacheDirty = true;
            saveToFile();
            return true;
        }
    }
    return false;
}

bool SignalStorage::renameDevice(const std::string& oldName, const std::string& newName) {
    std::string oldTrim = trimString(oldName);
    std::string newTrim = trimString(newName);
    if (newTrim.empty() || oldTrim == newTrim || hasDevice(newTrim)) {
        return false;
    }
    auto dev = getDevice(oldTrim);
    if (!dev) return false;
    dev->name = newTrim;
    for (auto& sig : dev->savedSignals) {
        sig.rcdName = newTrim;
    }
    m_cacheDirty = true;
    saveToFile();
    return true;
}

size_t SignalStorage::deviceCount() const {
    return m_devices.size();
}

const std::vector<SavedIrSignal>& SignalStorage::getSignals() const {
    if (m_cacheDirty) {
        rebuildCache();
    }
    return m_cachedSignals;
}

std::vector<SavedIrSignal> SignalStorage::getSignals(const std::string& rcdName) const {
    auto dev = getDevice(rcdName);
    if (!dev) return {};
    auto res = dev->savedSignals;
    for (auto& s : res) {
        s.rcdName = dev->name;
    }
    return res;
}

std::optional<SavedIrSignal> SignalStorage::getSignal(size_t index) const {
    if (m_cacheDirty) {
        rebuildCache();
    }
    if (index < m_cachedSignals.size()) {
        return m_cachedSignals[index];
    }
    return std::nullopt;
}

std::optional<SavedIrSignal> SignalStorage::getSignal(const std::string& rcdName, size_t signalIndex) const {
    auto dev = getDevice(rcdName);
    if (dev && signalIndex < dev->savedSignals.size()) {
        auto sig = dev->savedSignals[signalIndex];
        sig.rcdName = dev->name;
        return sig;
    }
    return std::nullopt;
}

size_t SignalStorage::count() const {
    size_t total = 0;
    for (const auto& dev : m_devices) {
        total += dev.savedSignals.size();
    }
    return total;
}

size_t SignalStorage::count(const std::string& rcdName) const {
    auto dev = getDevice(rcdName);
    return dev ? dev->savedSignals.size() : 0;
}

void SignalStorage::addSignal(const SavedIrSignal& signal, const std::string& rcdName) {
    std::string targetDevice = trimString(rcdName);
    if (targetDevice.empty()) {
        targetDevice = trimString(signal.rcdName);
    }
    if (targetDevice.empty()) {
        targetDevice = "Default Remote Device";
    }

    RemoteControlDevice* dev = getDevice(targetDevice);
    if (!dev) {
        m_devices.push_back(RemoteControlDevice{targetDevice, {}});
        dev = &m_devices.back();
    }

    SavedIrSignal stored = signal;
    stored.rcdName = targetDevice;
    dev->savedSignals.push_back(std::move(stored));
    m_cacheDirty = true;
    saveToFile();
}

void SignalStorage::addSignal(const std::string& rcdName, const SavedIrSignal& signal) {
    addSignal(signal, rcdName);
}

bool SignalStorage::updateSignal(size_t index, const SavedIrSignal& signal) {
    size_t current = 0;
    for (size_t d = 0; d < m_devices.size(); ++d) {
        auto& dev = m_devices[d];
        if (index < current + dev.savedSignals.size()) {
            size_t sigIdx = index - current;
            std::string targetRcd = trimString(signal.rcdName);
            if (targetRcd.empty()) {
                targetRcd = dev.name;
            }

            if (targetRcd != dev.name) {
                // Signal moved to a different RCD
                SavedIrSignal updated = signal;
                updated.rcdName = targetRcd;
                dev.savedSignals.erase(dev.savedSignals.begin() + sigIdx);
                if (dev.savedSignals.empty()) {
                    m_devices.erase(m_devices.begin() + d);
                }
                addSignal(updated, targetRcd);
            } else {
                dev.savedSignals[sigIdx] = signal;
                dev.savedSignals[sigIdx].rcdName = dev.name;
                m_cacheDirty = true;
                saveToFile();
            }
            return true;
        }
        current += dev.savedSignals.size();
    }
    return false;
}

bool SignalStorage::updateSignal(const std::string& rcdName, size_t signalIndex, const SavedIrSignal& signal) {
    auto dev = getDevice(rcdName);
    if (!dev || signalIndex >= dev->savedSignals.size()) {
        return false;
    }

    std::string targetRcd = trimString(signal.rcdName);
    if (targetRcd.empty()) targetRcd = dev->name;

    if (targetRcd != dev->name) {
        SavedIrSignal updated = signal;
        updated.rcdName = targetRcd;
        dev->savedSignals.erase(dev->savedSignals.begin() + signalIndex);
        if (dev->savedSignals.empty()) {
            removeDevice(rcdName);
        }
        addSignal(updated, targetRcd);
    } else {
        dev->savedSignals[signalIndex] = signal;
        dev->savedSignals[signalIndex].rcdName = dev->name;
        m_cacheDirty = true;
        saveToFile();
    }
    return true;
}

bool SignalStorage::removeSignal(size_t index) {
    size_t current = 0;
    for (size_t d = 0; d < m_devices.size(); ++d) {
        auto& dev = m_devices[d];
        if (index < current + dev.savedSignals.size()) {
            size_t sigIdx = index - current;
            dev.savedSignals.erase(dev.savedSignals.begin() + sigIdx);
            // Each RCD contains 1 .. n savedSignals: remove device if empty
            if (dev.savedSignals.empty()) {
                m_devices.erase(m_devices.begin() + d);
            }
            m_cacheDirty = true;
            saveToFile();
            return true;
        }
        current += dev.savedSignals.size();
    }
    return false;
}

bool SignalStorage::removeSignal(const std::string& rcdName, size_t signalIndex) {
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->name == rcdName) {
            if (signalIndex < it->savedSignals.size()) {
                it->savedSignals.erase(it->savedSignals.begin() + signalIndex);
                if (it->savedSignals.empty()) {
                    m_devices.erase(it);
                }
                m_cacheDirty = true;
                saveToFile();
                return true;
            }
            return false;
        }
    }
    return false;
}

void SignalStorage::clear() {
    m_devices.clear();
    m_cachedSignals.clear();
    m_cacheDirty = true;
    saveToFile();
}

bool SignalStorage::saveToFile(const std::string& path) const {
    std::string targetPath = path.empty() ? m_storagePath : path;
    std::filesystem::path targetFile(targetPath);
    if (targetFile.has_parent_path()) {
        std::filesystem::create_directories(targetFile.parent_path());
    }

    std::ofstream outFile(targetPath);
    if (!outFile.is_open()) {
        return false;
    }

    nlohmann::json root = nlohmann::json::array();
    for (const auto& dev : m_devices) {
        // Each RCD contains 1 .. n savedSignals
        if (dev.savedSignals.empty()) {
            continue;
        }

        nlohmann::json devJson;
        devJson["name"] = dev.name;
        nlohmann::json sigsJson = nlohmann::json::array();

        for (const auto& sig : dev.savedSignals) {
            nlohmann::json s;
            s["name"] = sig.name;
            s["protocol"] = sig.protocol;
            s["frequency"] = static_cast<int>(sig.frequency_khz);
            s["timestamp"] = sig.timestamp;
            s["rawHex"] = sig.rawHex;
            s["pulses"] = sig.pulses;
            if (!sig.shellCommand.empty()) {
                s["shellCommand"] = sig.shellCommand;
            }
            sigsJson.push_back(std::move(s));
        }

        devJson["signals"] = std::move(sigsJson);
        root.push_back(std::move(devJson));
    }

    outFile << root.dump(2) << "\n";
    return true;
}

static SavedIrSignal parseSignalJson(const nlohmann::json& s, const std::string& defaultRcd) {
    SavedIrSignal sig;
    sig.name = s.value("name", "");
    sig.protocol = s.value("protocol", "Raw");
    sig.frequency_khz = static_cast<uint8_t>(s.value("frequency", 38));
    sig.timestamp = s.value("timestamp", "");
    sig.rawHex = s.value("rawHex", "N/A");
    sig.rcdName = s.value("rcdName", defaultRcd);
    if (s.contains("shellCommand") && s["shellCommand"].is_string()) {
        sig.shellCommand = s["shellCommand"].get<std::string>();
    } else if (s.contains("shell_command") && s["shell_command"].is_string()) {
        sig.shellCommand = s["shell_command"].get<std::string>();
    }
    if (s.contains("pulses") && s["pulses"].is_array()) {
        sig.pulses = s["pulses"].get<std::vector<int>>();
    }
    return sig;
}

bool SignalStorage::loadFromFile(const std::string& path) {
    std::string targetPath = path.empty() ? m_storagePath : path;
    std::ifstream inFile(targetPath);
    if (!inFile.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(inFile);
    } catch (...) {
        return false;
    }

    m_devices.clear();

    if (root.is_array()) {
        if (root.empty()) {
            m_cacheDirty = true;
            return true;
        }

        // Check if elements are new format (contain "signals" or "savedSignals") or old format (flat signals)
        bool isNewFormat = false;
        for (const auto& item : root) {
            if (item.is_object() && (item.contains("signals") || item.contains("savedSignals"))) {
                isNewFormat = true;
                break;
            }
        }

        if (isNewFormat) {
            for (const auto& item : root) {
                if (!item.is_object()) continue;
                std::string devName = trimString(item.value("name", "Default Remote Device"));
                if (devName.empty()) devName = "Default Remote Device";

                RemoteControlDevice dev;
                dev.name = devName;

                std::string sigKey;
                if (item.contains("signals") && item["signals"].is_array()) {
                    sigKey = "signals";
                } else if (item.contains("savedSignals") && item["savedSignals"].is_array()) {
                    sigKey = "savedSignals";
                }

                if (!sigKey.empty()) {
                    for (const auto& s : item[sigKey]) {
                        SavedIrSignal sig = parseSignalJson(s, devName);
                        sig.rcdName = devName;
                        if (!sig.pulses.empty() || !sig.name.empty()) {
                            dev.savedSignals.push_back(std::move(sig));
                        }
                    }
                }

                if (!dev.savedSignals.empty()) {
                    m_devices.push_back(std::move(dev));
                }
            }
        } else {
            // Old format: move all signals into "Default Remote Device"
            RemoteControlDevice defaultDev;
            defaultDev.name = "Default Remote Device";

            for (const auto& item : root) {
                if (!item.is_object()) continue;
                SavedIrSignal sig = parseSignalJson(item, defaultDev.name);
                sig.rcdName = defaultDev.name;
                if (!sig.pulses.empty() || !sig.name.empty()) {
                    defaultDev.savedSignals.push_back(std::move(sig));
                }
            }

            if (!defaultDev.savedSignals.empty()) {
                m_devices.push_back(std::move(defaultDev));
            }
        }
    } else if (root.is_object()) {
        // Check for wrapper objects like {"remoteControlDevices": [...]} or {"devices": [...]}
        std::string key;
        if (root.contains("remoteControlDevices") && root["remoteControlDevices"].is_array()) {
            key = "remoteControlDevices";
        } else if (root.contains("devices") && root["devices"].is_array()) {
            key = "devices";
        }

        if (!key.empty()) {
            for (const auto& item : root[key]) {
                if (!item.is_object()) continue;
                std::string devName = trimString(item.value("name", "Default Remote Device"));
                if (devName.empty()) devName = "Default Remote Device";

                RemoteControlDevice dev;
                dev.name = devName;
                std::string sigKey;
                if (item.contains("signals") && item["signals"].is_array()) {
                    sigKey = "signals";
                } else if (item.contains("savedSignals") && item["savedSignals"].is_array()) {
                    sigKey = "savedSignals";
                }
                if (!sigKey.empty()) {
                    for (const auto& s : item[sigKey]) {
                        SavedIrSignal sig = parseSignalJson(s, devName);
                        sig.rcdName = devName;
                        if (!sig.pulses.empty() || !sig.name.empty()) {
                            dev.savedSignals.push_back(std::move(sig));
                        }
                    }
                }
                if (!dev.savedSignals.empty()) {
                    m_devices.push_back(std::move(dev));
                }
            }
        } else if ((root.contains("signals") && root["signals"].is_array()) ||
                   (root.contains("savedSignals") && root["savedSignals"].is_array())) {
            // Old format inside object
            std::string sigKey = root.contains("signals") ? "signals" : "savedSignals";
            RemoteControlDevice defaultDev;
            defaultDev.name = "Default Remote Device";
            for (const auto& s : root[sigKey]) {
                SavedIrSignal sig = parseSignalJson(s, defaultDev.name);
                sig.rcdName = defaultDev.name;
                if (!sig.pulses.empty() || !sig.name.empty()) {
                    defaultDev.savedSignals.push_back(std::move(sig));
                }
            }
            if (!defaultDev.savedSignals.empty()) {
                m_devices.push_back(std::move(defaultDev));
            }
        }
    }

    m_cacheDirty = true;
    return true;
}

static bool hexStringsMatch(const std::string& h1, const std::string& h2) {
    if (h1.empty() || h2.empty() || h1 == "N/A" || h2 == "N/A") {
        return false;
    }
    std::string s1 = h1;
    std::string s2 = h2;
    if (s1.rfind("0x", 0) == 0 || s1.rfind("0X", 0) == 0) s1 = s1.substr(2);
    if (s2.rfind("0x", 0) == 0 || s2.rfind("0X", 0) == 0) s2 = s2.substr(2);
    if (s1.size() != s2.size()) return false;
    for (size_t i = 0; i < s1.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s1[i])) != std::tolower(static_cast<unsigned char>(s2[i]))) {
            return false;
        }
    }
    return true;
}

static bool matchSignalPulses(const SavedIrSignal& saved,
                             const std::vector<int>& pulses,
                             const std::vector<int>& normalizedIncoming,
                             bool isIncomingNec,
                             const DecodedCode& incomingNec,
                             const std::string& incomingHexStr) {
    std::vector<int> savedNormalized = saved.pulses;
    IrProtocol::fixLeadingMark(savedNormalized);

    // 1. Check NEC decoded code match
    if (isIncomingNec) {
        if (!saved.rawHex.empty() && saved.rawHex != "N/A") {
            if (hexStringsMatch(saved.rawHex, incomingHexStr)) {
                return true;
            }
        }

        DecodedCode savedNec;
        if (IrProtocol::decodeNec(savedNormalized, savedNec)) {
            if (savedNec.rawValue == incomingNec.rawValue && savedNec.isRepeat == incomingNec.isRepeat) {
                return true;
            }
        }
    }

    // 2. Exact pulse match
    if (saved.pulses == pulses || saved.pulses == normalizedIncoming || savedNormalized == normalizedIncoming) {
        return true;
    }

    // 3. Timing-tolerant pulse sequence match
    if (savedNormalized.size() >= 4 && normalizedIncoming.size() >= 4) {
        int lenDiff = std::abs(static_cast<int>(savedNormalized.size()) - static_cast<int>(normalizedIncoming.size()));
        if (lenDiff <= 2) {
            size_t compLen = std::min(savedNormalized.size(), normalizedIncoming.size());
            bool match = true;
            for (size_t i = 0; i < compLen; ++i) {
                int p1 = normalizedIncoming[i];
                int p2 = savedNormalized[i];

                if ((p1 > 0) != (p2 > 0)) {
                    match = false;
                    break;
                }

                if (i == compLen - 1 && p1 < -10000 && p2 < -10000) {
                    continue;
                }

                int diff = std::abs(std::abs(p1) - std::abs(p2));
                int maxVal = std::max(std::abs(p1), std::abs(p2));
                int tol = std::max(200, static_cast<int>(0.25f * maxVal));
                if (diff > tol) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return true;
            }
        }
    }

    return false;
}

std::optional<SavedIrSignal> SignalStorage::findMatchingSignal(const IrSignal& signal) const {
    return findMatchingSignal(signal.pulses, signal.frequency_khz);
}

std::optional<SavedIrSignal> SignalStorage::findMatchingSignal(const std::vector<int>& pulses, uint8_t frequency_khz) const {
    (void)frequency_khz;
    if (pulses.empty() || m_devices.empty()) {
        return std::nullopt;
    }

    std::vector<int> normalizedIncoming = pulses;
    IrProtocol::fixLeadingMark(normalizedIncoming);

    DecodedCode incomingNec;
    bool isIncomingNec = IrProtocol::decodeNec(normalizedIncoming, incomingNec);
    std::string incomingHexStr;
    if (isIncomingNec) {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << incomingNec.rawValue;
        incomingHexStr = ss.str();
    }

    for (const auto& dev : m_devices) {
        for (const auto& saved : dev.savedSignals) {
            if (matchSignalPulses(saved, pulses, normalizedIncoming, isIncomingNec, incomingNec, incomingHexStr)) {
                SavedIrSignal result = saved;
                result.rcdName = dev.name;
                return result;
            }
        }
    }

    return std::nullopt;
}

std::optional<SavedIrSignal> SignalStorage::findMatchingSignalInDevice(const std::string& rcdName, const IrSignal& signal) const {
    return findMatchingSignalInDevice(rcdName, signal.pulses, signal.frequency_khz);
}

std::optional<SavedIrSignal> SignalStorage::findMatchingSignalInDevice(const std::string& rcdName, const std::vector<int>& pulses, uint8_t frequency_khz) const {
    (void)frequency_khz;
    auto dev = getDevice(rcdName);
    if (!dev || pulses.empty()) {
        return std::nullopt;
    }

    std::vector<int> normalizedIncoming = pulses;
    IrProtocol::fixLeadingMark(normalizedIncoming);

    DecodedCode incomingNec;
    bool isIncomingNec = IrProtocol::decodeNec(normalizedIncoming, incomingNec);
    std::string incomingHexStr;
    if (isIncomingNec) {
        std::ostringstream ss;
        ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << incomingNec.rawValue;
        incomingHexStr = ss.str();
    }

    for (const auto& saved : dev->savedSignals) {
        if (matchSignalPulses(saved, pulses, normalizedIncoming, isIncomingNec, incomingNec, incomingHexStr)) {
            SavedIrSignal result = saved;
            result.rcdName = dev->name;
            return result;
        }
    }

    return std::nullopt;
}

} // namespace ir
