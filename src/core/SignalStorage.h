#pragma once

#include "IrProtocol.h"
#include <string>
#include <vector>
#include <optional>

namespace ir {

struct SavedIrSignal {
    std::string name;
    std::string protocol{"Raw"};
    uint8_t frequency_khz{38};
    std::string timestamp;
    std::vector<int> pulses;
    std::string rawHex;
    std::string rcdName{"Default Remote Device"};

    std::string toJsonString() const;
};

struct RemoteControlDevice {
    std::string name;
    std::vector<SavedIrSignal> savedSignals;
};

using RCD = RemoteControlDevice;

class SignalStorage {
public:
    static std::string getDefaultStoragePath();

    explicit SignalStorage(std::string storagePath = "");

    bool loadFromFile(const std::string& path = "");
    bool saveToFile(const std::string& path = "") const;

    // Remote Control Device (RCD) management
    const std::vector<RemoteControlDevice>& getDevices() const;
    std::vector<RemoteControlDevice>& getDevices();
    std::vector<std::string> getDeviceNames() const;
    bool hasDevice(const std::string& name) const;
    const RemoteControlDevice* getDevice(const std::string& name) const;
    RemoteControlDevice* getDevice(const std::string& name);
    bool addDevice(const std::string& name);
    bool removeDevice(const std::string& name);
    bool renameDevice(const std::string& oldName, const std::string& newName);
    size_t deviceCount() const;

    // Signal management
    void addSignal(const SavedIrSignal& signal, const std::string& rcdName = "");
    void addSignal(const std::string& rcdName, const SavedIrSignal& signal);
    bool updateSignal(size_t index, const SavedIrSignal& signal);
    bool updateSignal(const std::string& rcdName, size_t signalIndex, const SavedIrSignal& signal);
    bool removeSignal(size_t index);
    bool removeSignal(const std::string& rcdName, size_t signalIndex);
    void clear();

    const std::vector<SavedIrSignal>& getSignals() const;
    std::vector<SavedIrSignal> getSignals(const std::string& rcdName) const;
    std::optional<SavedIrSignal> getSignal(size_t index) const;
    std::optional<SavedIrSignal> getSignal(const std::string& rcdName, size_t signalIndex) const;
    size_t count() const;
    size_t count(const std::string& rcdName) const;
    const std::string& getStoragePath() const;

    // Matches an incoming signal against stored signals.
    // Returns matching SavedIrSignal if found, or std::nullopt.
    std::optional<SavedIrSignal> findMatchingSignal(const IrSignal& signal) const;
    std::optional<SavedIrSignal> findMatchingSignal(const std::vector<int>& pulses, uint8_t frequency_khz = 38) const;
    std::optional<SavedIrSignal> findMatchingSignalInDevice(const std::string& rcdName, const IrSignal& signal) const;
    std::optional<SavedIrSignal> findMatchingSignalInDevice(const std::string& rcdName, const std::vector<int>& pulses, uint8_t frequency_khz = 38) const;

private:
    void rebuildCache() const;

    std::string m_storagePath;
    std::vector<RemoteControlDevice> m_devices;
    mutable std::vector<SavedIrSignal> m_cachedSignals;
    mutable bool m_cacheDirty{true};
};

} // namespace ir
