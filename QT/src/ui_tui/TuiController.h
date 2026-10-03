#pragma once

#include "IrUsbTransceiver.h"
#include "IrProtocol.h"
#include "SignalStorage.h"
#include <memory>
#include <optional>

namespace ir {

class TuiController {
public:
    explicit TuiController(std::shared_ptr<IrUsbTransceiver> transceiver = nullptr);
    ~TuiController() = default;

    void runInteractiveLoop();

    // Mode operations
    bool captureOnce(float timeoutSeconds = 10.0f);
    void startContinuousMonitor();
    void saveLastCapturedSignal();
    void listSavedSignals();
    void transmitSavedSignal();
    void transmitManualSequence();
    void printHardwareInfo();
    void simulateCapture();

    const std::optional<IrSignal>& getLastCapturedSignal() const { return m_lastCaptured; }

private:
    void printBanner() const;
    void printMenu() const;
    void displaySignalDetails(const IrSignal& signal, const std::string& title = "CAPTURED IR SIGNAL");

    std::shared_ptr<IrUsbTransceiver> m_transceiver;
    SignalStorage m_storage;
    std::optional<IrSignal> m_lastCaptured;
};

} // namespace ir
