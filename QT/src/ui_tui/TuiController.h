#pragma once

#include "IrProtocol.h"
#include "SignalStorage.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <optional>
#include <atomic>

namespace ir {

class TuiController {
public:
    explicit TuiController(std::string apiBaseUrl = "http://127.0.0.1:8080");
    TuiController(std::string host, int port);
    ~TuiController() = default;

    void runInteractiveLoop();

    // Mode operations
    bool captureOnce(float timeoutSeconds = 10.0f);
    void startContinuousMonitor();
    void saveLastCapturedSignal();
    void listSavedSignals();
    void transmitSavedSignal();
    void transmitManualSequence();
    bool transmitPulses(const std::vector<int>& pulses, uint8_t frequency_khz = 38, const std::string& name = "Custom Signal");
    void printHardwareInfo();
    void simulateCapture();

    bool isApiReachable();
    bool isDeviceConnected();

    const std::string& getApiBaseUrl() const { return m_apiBaseUrl; }
    void setApiBaseUrl(const std::string& url);

    const std::optional<IrSignal>& getLastCapturedSignal() const { return m_lastCaptured; }
    const SignalStorage& getStorage() const { return m_storage; }
    SignalStorage& getStorage() { return m_storage; }

private:
    void printBanner() const;
    void printMenu();
    void displaySignalDetails(const IrSignal& signal, const std::string& title = "CAPTURED IR SIGNAL", const std::string& identifiedName = "");
    void handleReceivedSignal(const IrSignal& sig, const std::string& title = "NEW IR SIGNAL CAPTURED", bool promptIfNew = true);
    std::unique_ptr<httplib::Client> createClient(int timeoutSeconds = 5) const;

    std::string m_apiBaseUrl{"http://127.0.0.1:8080"};
    std::string m_host{"127.0.0.1"};
    int m_port{8080};
    SignalStorage m_storage;
    std::optional<IrSignal> m_lastCaptured;
    std::atomic<bool> m_monitoring{false};
};

} // namespace ir
