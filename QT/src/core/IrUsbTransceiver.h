#pragma once

#include "IrProtocol.h"
#include "RingBuffer.h"

#include <libusb-1.0/libusb.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace ir {

enum class DeviceState {
    Disconnected,
    Idle,
    Receiving,
    Transmitting,
    Error
};

struct DeviceInfo {
    std::string manufacturer;
    std::string product;
    std::string serialNumber;
    uint16_t vendorId{0x10C4};
    uint16_t productId{0x8468};
    bool isConnected{false};
    uint8_t defaultFrequency{38};
};

/**
 * @brief High-performance C++ driver engine for the Tiqiaa Tview USB IR Transceiver.
 * Uses native libusb-1.0 for cross-platform hardware access across Linux, Windows, and Android.
 */
class IrUsbTransceiver {
public:
    using SignalReceivedCallback = std::function<void(const IrSignal&)>;
    using StateChangedCallback = std::function<void(DeviceState)>;
    using LogCallback = std::function<void(const std::string&)>;

    explicit IrUsbTransceiver(uint8_t defaultFrequency = 38);
    ~IrUsbTransceiver();

    // Prevent copy, allow move if needed
    IrUsbTransceiver(const IrUsbTransceiver&) = delete;
    IrUsbTransceiver& operator=(const IrUsbTransceiver&) = delete;

    /**
     * @brief Opens the Tiqiaa USB device natively using VID (0x10C4) and PID (0x8468).
     */
    bool openDevice();

    /**
     * @brief Opens the USB device using an existing Android file descriptor (JNI / NDK).
     * @param sysFd Android UsbDeviceConnection file descriptor.
     */
    bool openFromSysDevice(intptr_t sysFd);

    /**
     * @brief Closes device connection and disposes libusb resources.
     */
    void closeDevice();

    /**
     * @brief Checks if USB device is open and claimed.
     */
    bool isConnected() const;

    /**
     * @brief Returns current state of transceiver.
     */
    DeviceState getState() const;

    /**
     * @brief Gets detailed hardware hardware information.
     */
    DeviceInfo getDeviceInfo() const;

    /**
     * @brief Switches transceiver into Idle / Standby mode ('L').
     */
    bool setIdle();

    /**
     * @brief Switches transceiver into Receive mode ('R').
     */
    bool startReceive();

    /**
     * @brief Switches transceiver into Transmit mode ('S').
     */
    bool startTransmit();

    /**
     * @brief Transmits a pulse sequence (microseconds).
     * @param pulses Array of pulse durations (+ = ON, - = OFF)
     * @param frequency_khz Carrier frequency in kHz (0 uses default frequency)
     */
    bool transmitIr(const std::vector<int>& pulses, uint8_t frequency_khz = 0);

    /**
     * @brief Transmits an IrSignal struct.
     */
    bool transmitIr(const IrSignal& signal);

    /**
     * @brief Performs a single blocking capture attempt with timeout.
     * @param outSignal Output signal parameter
     * @param timeoutSeconds Maximum time to wait in seconds
     */
    bool captureOnce(IrSignal& outSignal, float timeoutSeconds = 10.0f);

    /**
     * @brief Starts background receive polling thread.
     */
    void startBackgroundPolling();

    /**
     * @brief Stops background receive polling thread.
     */
    void stopBackgroundPolling();

    /**
     * @brief Pops next received IR signal from internal thread-safe ring buffer.
     */
    std::optional<IrSignal> popReceivedSignal();

    /**
     * @brief Queries firmware version string from hardware ('V').
     */
    std::string getFirmwareVersion();

    // Callbacks
    void setSignalReceivedCallback(SignalReceivedCallback cb);
    void setStateChangedCallback(StateChangedCallback cb);
    void setLogCallback(LogCallback cb);

    // Low-level command & USB transfers (protected by m_usbMutex)
    bool sendCommand(char cmd, const std::vector<uint8_t>& extraBytes = {});
    bool sendBulk(const std::vector<uint8_t>& data);
    bool readData(std::vector<uint8_t>& outData, unsigned int timeoutMs = 1000);
    bool readPayload(std::vector<uint8_t>& outPayload, unsigned int timeoutMs = 1000);
    void flushInputBuffer();

    static constexpr uint16_t VENDOR_ID = 0x10C4;
    static constexpr uint16_t PRODUCT_ID = 0x8468;
    static constexpr uint8_t EP_OUT = 0x01;
    static constexpr uint8_t EP_IN = 0x81;
    static constexpr int INTERFACE_NUM = 0;

private:
    void setState(DeviceState state);
    void log(const std::string& msg);
    void pollingLoop();
    bool transferOut(const std::vector<uint8_t>& data, unsigned int timeoutMs);

    libusb_context* m_ctx{nullptr};
    libusb_device_handle* m_devHandle{nullptr};
    mutable std::mutex m_usbMutex;

    DeviceState m_state{DeviceState::Disconnected};
    uint8_t m_cmdId{1};
    uint8_t m_usbPackCounter{1};
    uint8_t m_defaultFrequency{38};

    DeviceInfo m_info;

    // Background thread state
    std::atomic<bool> m_pollingActive{false};
    std::thread m_pollingThread;
    RingBuffer<IrSignal, 32> m_rxRingBuffer;

    // Callbacks
    SignalReceivedCallback m_onSignalReceived;
    StateChangedCallback m_onStateChanged;
    LogCallback m_onLog;
};

} // namespace ir
