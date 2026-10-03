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
    explicit SignalStorage(std::string storagePath = "saved_ir_signals.json");

    bool loadFromFile(const std::string& path = "");
    bool saveToFile(const std::string& path = "") const;

    void addSignal(const SavedIrSignal& signal);
    bool removeSignal(size_t index);
    void clear();

    const std::vector<SavedIrSignal>& getSignals() const;
    std::optional<SavedIrSignal> getSignal(size_t index) const;
    size_t count() const;
    const std::string& getStoragePath() const;

private:
    std::string m_storagePath;
    std::vector<SavedIrSignal> m_signals;
};

} // namespace ir
