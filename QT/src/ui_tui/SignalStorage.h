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

    std::string toJsonString() const;
};

class SignalStorage {
public:
    static std::string getDefaultStoragePath();

    explicit SignalStorage(std::string storagePath = "");

    bool loadFromFile(const std::string& path = "");
    bool saveToFile(const std::string& path = "") const;

    void addSignal(const SavedIrSignal& signal);
    bool removeSignal(size_t index);
    void clear();

    const std::vector<SavedIrSignal>& getSignals() const;
    std::optional<SavedIrSignal> getSignal(size_t index) const;
    size_t count() const;
    const std::string& getStoragePath() const;

    // Matches an incoming signal against stored signals.
    // Returns matching SavedIrSignal if found, or std::nullopt.
    std::optional<SavedIrSignal> findMatchingSignal(const IrSignal& signal) const;
    std::optional<SavedIrSignal> findMatchingSignal(const std::vector<int>& pulses, uint8_t frequency_khz = 38) const;

private:
    std::string m_storagePath;
    std::vector<SavedIrSignal> m_signals;
};

} // namespace ir
