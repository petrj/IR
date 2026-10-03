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
    const SignalStorage& getStorage() const { return m_storage; }
    SignalStorage& getStorage() { return m_storage; }

private:
    void printBanner() const;
    void printMenu() const;
    void displaySignalDetails(const IrSignal& signal, const std::string& title = "CAPTURED IR SIGNAL", const std::string& identifiedName = "");
    void handleReceivedSignal(const IrSignal& sig, const std::string& title = "NEW IR SIGNAL CAPTURED", bool promptIfNew = true);

    std::shared_ptr<IrUsbTransceiver> m_transceiver;
    SignalStorage m_storage;
    std::optional<IrSignal> m_lastCaptured;
};

} // namespace ir
