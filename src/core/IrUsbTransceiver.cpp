#include "IrUsbTransceiver.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <algorithm>

namespace ir {

static std::string toHexByte(uint8_t b) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02X", b);
    return std::string(buf);
}

IrUsbTransceiver::IrUsbTransceiver(uint8_t defaultFrequency)
    : m_defaultFrequency(defaultFrequency) {
    m_info.defaultFrequency = defaultFrequency;
    int res = libusb_init(&m_ctx);
    if (res < 0) {
        log("Failed to initialize libusb: " + std::string(libusb_strerror(static_cast<libusb_error>(res))));
    }
}

IrUsbTransceiver::~IrUsbTransceiver() {
    closeDevice();
    if (m_ctx) {
        libusb_exit(m_ctx);
        m_ctx = nullptr;
    }
}

void IrUsbTransceiver::log(const std::string& msg) {
    if (m_onLog) {
        m_onLog(msg);
    }
}

void IrUsbTransceiver::setState(DeviceState state) {
    if (m_state != state) {
        m_state = state;
        if (m_onStateChanged) {
            m_onStateChanged(m_state);
        }
    }
}

bool IrUsbTransceiver::openDevice() {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (m_devHandle) {
        return true;
    }

    if (!m_ctx) {
        log("libusb context is null");
        return false;
    }

    libusb_device** devList = nullptr;
    ssize_t cnt = libusb_get_device_list(m_ctx, &devList);
    if (cnt < 0) {
        log("Failed to get USB device list: " + std::string(libusb_strerror(static_cast<libusb_error>(cnt))));
        return false;
    }

    libusb_device* targetDev = nullptr;
    for (ssize_t i = 0; i < cnt; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devList[i], &desc) == 0) {
            if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
                targetDev = devList[i];
                break;
            }
        }
    }

    if (!targetDev) {
        log("Tiqiaa USB IR device not found on USB bus (VID: 0x10C4, PID: 0x8468)");
        libusb_free_device_list(devList, 1);
        setState(DeviceState::Disconnected);
        return false;
    }

    int openRes = libusb_open(targetDev, &m_devHandle);
    libusb_free_device_list(devList, 1);

    if (openRes < 0) {
        std::string err = libusb_strerror(static_cast<libusb_error>(openRes));
        if (openRes == LIBUSB_ERROR_ACCESS) {
            log("PERMISSION DENIED accessing USB device (VID: 0x10C4, PID: 0x8468). Run udev rule to grant access!");
        } else {
            log("Failed to open USB device: " + err);
        }
        setState(DeviceState::Error);
        return false;
    }

    log("Device opened successfully (VID: 0x10C4, PID: 0x8468)");

    // Detach kernel driver if active (Linux)
    if (libusb_kernel_driver_active(m_devHandle, INTERFACE_NUM) == 1) {
        log("Detaching kernel driver...");
        libusb_detach_kernel_driver(m_devHandle, INTERFACE_NUM);
    }

    int res = libusb_claim_interface(m_devHandle, INTERFACE_NUM);
    if (res < 0) {
        log("Failed to claim USB interface 0: " + std::string(libusb_strerror(static_cast<libusb_error>(res))));
        libusb_close(m_devHandle);
        m_devHandle = nullptr;
        setState(DeviceState::Error);
        return false;
    }

    // Query device string descriptors
    libusb_device* dev = libusb_get_device(m_devHandle);
    libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) == 0) {
        m_info.vendorId = desc.idVendor;
        m_info.productId = desc.idProduct;

        unsigned char stringBuf[256];
        if (desc.iManufacturer > 0 && libusb_get_string_descriptor_ascii(m_devHandle, desc.iManufacturer, stringBuf, sizeof(stringBuf)) > 0) {
            m_info.manufacturer = reinterpret_cast<char*>(stringBuf);
        } else {
            m_info.manufacturer = "Silicon Labs";
        }

        if (desc.iProduct > 0 && libusb_get_string_descriptor_ascii(m_devHandle, desc.iProduct, stringBuf, sizeof(stringBuf)) > 0) {
            m_info.product = reinterpret_cast<char*>(stringBuf);
        } else {
            m_info.product = "Tiqiaa Tview USB IR Transceiver";
        }

        if (desc.iSerialNumber > 0 && libusb_get_string_descriptor_ascii(m_devHandle, desc.iSerialNumber, stringBuf, sizeof(stringBuf)) > 0) {
            m_info.serialNumber = reinterpret_cast<char*>(stringBuf);
        } else {
            m_info.serialNumber = "N/A";
        }
    }

    discoverEndpoints();
    m_info.isConnected = true;
    m_cmdId = 1;
    m_usbPackCounter = 1;

    setState(DeviceState::Idle);
    log("Interface claimed successfully. Device Ready.");
    flushInputBuffer();

    return true;
}

void IrUsbTransceiver::discoverEndpoints() {
    if (!m_devHandle) return;

    libusb_device* dev = libusb_get_device(m_devHandle);
    if (!dev) return;

    libusb_config_descriptor* configDesc = nullptr;
    int res = libusb_get_active_config_descriptor(dev, &configDesc);
    if (res < 0 || !configDesc) {
        res = libusb_get_config_descriptor(dev, 0, &configDesc);
    }

    if (res == 0 && configDesc) {
        for (int i = 0; i < configDesc->bNumInterfaces; ++i) {
            const libusb_interface& iface = configDesc->interface[i];
            for (int a = 0; a < iface.num_altsetting; ++a) {
                const libusb_interface_descriptor& alt = iface.altsetting[a];
                if (alt.bInterfaceNumber == INTERFACE_NUM) {
                    for (int e = 0; e < alt.bNumEndpoints; ++e) {
                        const libusb_endpoint_descriptor& ep = alt.endpoint[e];
                        uint8_t xferType = ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
                        if ((ep.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
                            m_epIn = ep.bEndpointAddress;
                            m_epInType = xferType;
                            m_epInMaxPacketSize = ep.wMaxPacketSize;
                        } else {
                            m_epOut = ep.bEndpointAddress;
                            m_epOutType = xferType;
                            m_epOutMaxPacketSize = ep.wMaxPacketSize;
                        }
                    }
                }
            }
        }
        libusb_free_config_descriptor(configDesc);

        std::string inTypeStr = (m_epInType == LIBUSB_TRANSFER_TYPE_INTERRUPT) ? "INTERRUPT" :
                                (m_epInType == LIBUSB_TRANSFER_TYPE_BULK) ? "BULK" : "OTHER";
        std::string outTypeStr = (m_epOutType == LIBUSB_TRANSFER_TYPE_INTERRUPT) ? "INTERRUPT" :
                                 (m_epOutType == LIBUSB_TRANSFER_TYPE_BULK) ? "BULK" : "OTHER";

        log("Discovered USB Endpoints: IN=0x" + toHexByte(m_epIn) +
            " (" + inTypeStr + ", maxPacket=" + std::to_string(m_epInMaxPacketSize) + "B), OUT=0x" +
            toHexByte(m_epOut) + " (" + outTypeStr + ", maxPacket=" + std::to_string(m_epOutMaxPacketSize) + "B)");
    } else {
        log("Notice: Could not read active config descriptor (" +
            std::string(libusb_strerror(static_cast<libusb_error>(res))) +
            "), using defaults: EP_IN=0x81 (INTERRUPT), EP_OUT=0x01 (INTERRUPT)");
        m_epIn = EP_IN;
        m_epOut = EP_OUT;
        m_epInType = LIBUSB_TRANSFER_TYPE_INTERRUPT;
        m_epOutType = LIBUSB_TRANSFER_TYPE_INTERRUPT;
        m_epInMaxPacketSize = 64;
        m_epOutMaxPacketSize = 64;
    }
}

bool IrUsbTransceiver::openFromSysDevice(intptr_t sysFd) {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (m_devHandle) {
        return true;
    }

    if (!m_ctx) {
        return false;
    }

    int res = libusb_wrap_sys_device(m_ctx, sysFd, &m_devHandle);
    if (res < 0 || !m_devHandle) {
        log("Failed to wrap system device descriptor: " + std::string(libusb_strerror(static_cast<libusb_error>(res))));
        setState(DeviceState::Error);
        return false;
    }

    res = libusb_claim_interface(m_devHandle, INTERFACE_NUM);
    if (res < 0) {
        log("Failed to claim wrapped interface: " + std::string(libusb_strerror(static_cast<libusb_error>(res))));
        libusb_close(m_devHandle);
        m_devHandle = nullptr;
        setState(DeviceState::Error);
        return false;
    }

    discoverEndpoints();
    m_info.isConnected = true;
    m_info.manufacturer = "Tiqiaa (Android sysFd)";
    m_info.product = "Tiqiaa IR Transceiver";
    m_cmdId = 1;
    m_usbPackCounter = 1;

    setState(DeviceState::Idle);
    return true;
}

void IrUsbTransceiver::closeDevice() {
    stopBackgroundPolling();

    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (m_devHandle) {
        libusb_release_interface(m_devHandle, INTERFACE_NUM);
        libusb_close(m_devHandle);
        m_devHandle = nullptr;
    }
    m_info.isConnected = false;
    setState(DeviceState::Disconnected);
}

bool IrUsbTransceiver::isConnected() const {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    return m_devHandle != nullptr && m_info.isConnected;
}

DeviceState IrUsbTransceiver::getState() const {
    return m_state;
}

DeviceInfo IrUsbTransceiver::getDeviceInfo() const {
    return m_info;
}

bool IrUsbTransceiver::transferOut(const std::vector<uint8_t>& data, unsigned int timeoutMs) {
    if (!m_devHandle) return false;

    int actualLength = 0;
    int res = 0;

    // Use auto-discovered transfer type directly (Interrupt for HID CP2110, Bulk for CP2102/UART)
    if (m_epOutType == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
        res = libusb_interrupt_transfer(m_devHandle, m_epOut, const_cast<unsigned char*>(data.data()),
                                        static_cast<int>(data.size()), &actualLength, timeoutMs);
        if (res == LIBUSB_ERROR_INVALID_PARAM) {
            // Fallback to bulk if endpoint attribute was misreported
            res = libusb_bulk_transfer(m_devHandle, m_epOut, const_cast<unsigned char*>(data.data()),
                                       static_cast<int>(data.size()), &actualLength, timeoutMs);
        }
    } else {
        res = libusb_bulk_transfer(m_devHandle, m_epOut, const_cast<unsigned char*>(data.data()),
                                   static_cast<int>(data.size()), &actualLength, timeoutMs);
        if (res == LIBUSB_ERROR_INVALID_PARAM) {
            // Fallback to interrupt if endpoint attribute was misreported
            res = libusb_interrupt_transfer(m_devHandle, m_epOut, const_cast<unsigned char*>(data.data()),
                                            static_cast<int>(data.size()), &actualLength, timeoutMs);
        }
    }

    // Rigorous ARM USB stack error handling and clear-halt recovery
    if (res == LIBUSB_ERROR_PIPE) {
        log("[USB] OUT Endpoint 0x" + toHexByte(m_epOut) + " stalled (PIPE error). Clearing halt...");
        libusb_clear_halt(m_devHandle, m_epOut);
    } else if (res < 0 && res != LIBUSB_ERROR_TIMEOUT) {
        log("[USB] transferOut error on EP 0x" + toHexByte(m_epOut) + ": " +
            std::string(libusb_strerror(static_cast<libusb_error>(res))) +
            " (" + std::to_string(res) + ")");
    }

    return (res == 0 && actualLength == static_cast<int>(data.size()));
}

bool IrUsbTransceiver::sendBulk(const std::vector<uint8_t>& data) {
    if (!m_devHandle) return false;

    constexpr size_t maxChunkSize = 56;
    size_t totalChunks = (data.size() + maxChunkSize - 1) / maxChunkSize;
    if (totalChunks == 0) totalChunks = 1;

    for (size_t idx = 0; idx < totalChunks; ++idx) {
        size_t offset = idx * maxChunkSize;
        size_t chunkSize = std::min(maxChunkSize, data.size() - offset);

        std::vector<uint8_t> packet(5 + chunkSize);
        packet[0] = 0x02; // Report ID
        packet[1] = static_cast<uint8_t>(chunkSize + 3);
        packet[2] = m_usbPackCounter;
        packet[3] = static_cast<uint8_t>(totalChunks);
        packet[4] = static_cast<uint8_t>(idx + 1);

        std::copy(data.begin() + offset, data.begin() + offset + chunkSize, packet.begin() + 5);

        if (!transferOut(packet, 3000)) {
            log("Failed writing USB fragment " + std::to_string(idx + 1) + "/" + std::to_string(totalChunks));
            return false;
        }

        // Inter-fragment sleep required by device MCU (150ms matching tiqiaa_IR.py)
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    m_usbPackCounter++;
    if (m_usbPackCounter > 15) {
        m_usbPackCounter = 1;
    }

    return true;
}

bool IrUsbTransceiver::sendCommand(char cmd, const std::vector<uint8_t>& extraBytes) {
    std::vector<uint8_t> header;
    header.reserve(4 + extraBytes.size() + 2);
    header.push_back('S');
    header.push_back('T');
    header.push_back(m_cmdId);
    header.push_back(static_cast<uint8_t>(cmd));

    for (uint8_t b : extraBytes) {
        header.push_back(b);
    }

    header.push_back('E');
    header.push_back('N');

    bool ok = sendBulk(header);

    m_cmdId++;
    if (m_cmdId > 200) {
        m_cmdId = 1;
    }

    return ok;
}

bool IrUsbTransceiver::readData(std::vector<uint8_t>& outData, unsigned int timeoutMs) {
    if (!m_devHandle) return false;

    // Buffer padded to multiple of max packet size (e.g. 64 * 16 = 1024)
    size_t packetSize = m_epInMaxPacketSize > 0 ? m_epInMaxPacketSize : 64;
    size_t bufSize = ((1024 + packetSize - 1) / packetSize) * packetSize;
    std::vector<uint8_t> buffer(bufSize);
    int actualLength = 0;
    int res = 0;

    // Use auto-discovered transfer type directly (Interrupt for HID CP2110, Bulk for CP2102/UART)
    if (m_epInType == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
        res = libusb_interrupt_transfer(m_devHandle, m_epIn, buffer.data(),
                                        static_cast<int>(buffer.size()), &actualLength, timeoutMs);
        if (res == LIBUSB_ERROR_INVALID_PARAM) {
            res = libusb_bulk_transfer(m_devHandle, m_epIn, buffer.data(),
                                       static_cast<int>(buffer.size()), &actualLength, timeoutMs);
        }
    } else {
        res = libusb_bulk_transfer(m_devHandle, m_epIn, buffer.data(),
                                   static_cast<int>(buffer.size()), &actualLength, timeoutMs);
        if (res == LIBUSB_ERROR_INVALID_PARAM) {
            res = libusb_interrupt_transfer(m_devHandle, m_epIn, buffer.data(),
                                            static_cast<int>(buffer.size()), &actualLength, timeoutMs);
        }
    }

    // Rigorous ARM USB stack error handling and clear-halt recovery
    if (res == LIBUSB_ERROR_PIPE) {
        log("[USB] IN Endpoint 0x" + toHexByte(m_epIn) + " stalled (PIPE error). Clearing halt...");
        libusb_clear_halt(m_devHandle, m_epIn);
    } else if (res == LIBUSB_ERROR_OVERFLOW) {
        log("[USB] IN Endpoint 0x" + toHexByte(m_epIn) + " buffer overflow (OVERFLOW error). Clearing halt...");
        libusb_clear_halt(m_devHandle, m_epIn);
    } else if (res < 0 && res != LIBUSB_ERROR_TIMEOUT) {
        log("[USB] readData error on EP 0x" + toHexByte(m_epIn) + ": " +
            std::string(libusb_strerror(static_cast<libusb_error>(res))) +
            " (" + std::to_string(res) + ")");
    }

    if (res == 0 && actualLength > 0) {
        buffer.resize(actualLength);
        outData = std::move(buffer);
        return true;
    }

    outData.clear();
    return false;
}

void IrUsbTransceiver::flushInputBuffer() {
    log("Flushing stale USB input buffer...");
    std::vector<uint8_t> dummy;
    int flushedCount = 0;
    while (readData(dummy, 50)) {
        flushedCount += dummy.size();
    }
    if (flushedCount > 0) {
        log("Flushed " + std::to_string(flushedCount) + " stale bytes from USB input.");
    }
}

bool IrUsbTransceiver::readPayload(std::vector<uint8_t>& outPayload, unsigned int timeoutMs) {
    std::vector<uint8_t> firstPacket;
    if (!readData(firstPacket, timeoutMs)) {
        return false;
    }

    if (firstPacket.size() < 5) {
        outPayload = std::move(firstPacket);
        return true;
    }

    uint8_t reportId = firstPacket[0];
    uint8_t fragLen = (firstPacket.size() > 1 && firstPacket[1] >= 3) ? (firstPacket[1] - 3) : 0;
    uint8_t totalChunks = firstPacket[3];
    uint8_t chunkIdx = firstPacket[4];

    auto hasEnTerminator = [](const std::vector<uint8_t>& p) {
        if (p.size() < 2) return false;
        for (size_t i = p.size(); i >= 2; --i) {
            if (p[i - 2] == 'E' && p[i - 1] == 'N') return true;
        }
        return false;
    };

    if ((reportId == 0x01 || reportId == 0x02) && totalChunks > 1) {
        // If totalChunks > 1, the first packet of a frame MUST be chunk index 1.
        // If chunkIdx != 1, this is an orphaned fragment from a previous failed/interrupted frame.
        if (chunkIdx != 1) {
            log("Received orphaned USB fragment " + std::to_string(chunkIdx) + "/" +
                std::to_string(totalChunks) + " as first packet, discarding and flushing buffer");
            flushInputBuffer();
            return false;
        }

        std::vector<uint8_t> payload;
        size_t availableLen = std::min<size_t>(fragLen, firstPacket.size() - 5);
        payload.insert(payload.end(), firstPacket.begin() + 5, firstPacket.begin() + 5 + availableLen);

        int remaining = totalChunks - 1;
        int maxTimeouts = 4;
        int timeouts = 0;
        // The hardware MCU is slow when delivering trailing fragments.
        // We must use a dedicated trailing timeout of at least 1000ms (matching tiqiaa_IR.py)
        // rather than the short 300ms polling timeout.
        unsigned int trailingTimeoutMs = std::max(timeoutMs, 1000u);

        uint8_t expectedChunkIdx = 2;
        while (remaining > 0) {
            std::vector<uint8_t> nextPacket;
            if (readData(nextPacket, trailingTimeoutMs) && nextPacket.size() >= 5) {
                uint8_t nextReportId = nextPacket[0];
                if (nextReportId != 0x01 && nextReportId != 0x02) {
                    continue;
                }

                uint8_t nextLen = (nextPacket.size() > 1 && nextPacket[1] >= 3) ? (nextPacket[1] - 3) : 0;
                uint8_t nextChunk = nextPacket[4];
                if (nextChunk != expectedChunkIdx) {
                    log("Warning: Received chunk index " + std::to_string(nextChunk) +
                        " (expected " + std::to_string(expectedChunkIdx) + ")");
                }
                expectedChunkIdx = nextChunk + 1;

                size_t nextAvail = std::min<size_t>(nextLen, nextPacket.size() - 5);
                payload.insert(payload.end(), nextPacket.begin() + 5, nextPacket.begin() + 5 + nextAvail);
                remaining--;
                timeouts = 0;
            } else {
                timeouts++;
                if (timeouts >= maxTimeouts) {
                    log("Timed out waiting for trailing USB fragment (" + std::to_string(remaining) +
                        " chunks remaining, assembled payload: " + std::to_string(payload.size()) + " B)");
                    if (hasEnTerminator(payload)) {
                        break;
                    }
                    flushInputBuffer();
                    return false;
                }
            }
        }

        if (hasEnTerminator(payload)) {
            outPayload = std::move(payload);
            return true;
        } else {
            log("All fragments read but 'EN' terminator missing from payload");
            flushInputBuffer();
            return false;
        }
    } else {
        if ((reportId == 0x01 || reportId == 0x02) && firstPacket.size() >= 5) {
            size_t availableLen = std::min<size_t>(fragLen, firstPacket.size() - 5);
            outPayload.assign(firstPacket.begin() + 5, firstPacket.begin() + 5 + availableLen);
            return true;
        }
        outPayload = std::move(firstPacket);
        return true;
    }
}

std::string IrUsbTransceiver::getFirmwareVersion() {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return "";

    flushInputBuffer();
    sendCommand('V');
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<uint8_t> resp;
    if (readData(resp, 1000) && !resp.empty()) {
        std::string version;
        for (uint8_t b : resp) {
            if (b >= 32 && b <= 126) {
                version.push_back(static_cast<char>(b));
            }
        }
        size_t first = version.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) {
            size_t last = version.find_last_not_of(" \t\r\n");
            version = version.substr(first, last - first + 1);
        }
        return version.empty() ? "Unknown" : version;
    }
    return "Unknown";
}

bool IrUsbTransceiver::setIdle() {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return false;

    bool ok = sendCommand('L');
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<uint8_t> dummy;
    readData(dummy, 500);

    setState(DeviceState::Idle);
    return ok;
}

bool IrUsbTransceiver::startReceive() {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return false;

    flushInputBuffer();
    bool ok = sendCommand('R');
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::vector<uint8_t> dummy;
    readData(dummy, 500);

    setState(DeviceState::Receiving);
    log("Receive mode activated");
    return ok;
}

bool IrUsbTransceiver::startTransmit() {
    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return false;

    bool ok = sendCommand('S');
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<uint8_t> dummy;
    readData(dummy, 500);

    setState(DeviceState::Transmitting);
    log("Transmit mode activated");
    return ok;
}

bool IrUsbTransceiver::transmitIr(const std::vector<int>& pulses, uint8_t frequency_khz) {
    if (pulses.empty()) return false;
    uint8_t freq = (frequency_khz > 0 && frequency_khz != 0xFF) ? frequency_khz : m_defaultFrequency;
    if (freq == 0 || freq == 0xFF || freq > 60 || freq < 20) {
        freq = 38;
    }

    stopBackgroundPolling();

    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return false;

    setState(DeviceState::Transmitting);

    // Switch to transmit mode
    sendCommand('S');
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<uint8_t> dummy;
    readData(dummy, 300);

    auto extraBytes = IrProtocol::encodeTiqiaaPayload(pulses, freq);
    bool ok = sendCommand('D', extraBytes);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    readData(dummy, 500);

    // Return to standby mode
    sendCommand('L');
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    readData(dummy, 300);

    setState(DeviceState::Idle);
    log("Transmitted " + std::to_string(pulses.size()) + " pulses (" + std::to_string(freq) + " kHz)");
    return ok;
}

bool IrUsbTransceiver::transmitIr(const IrSignal& signal) {
    return transmitIr(signal.pulses, signal.frequency_khz);
}

bool IrUsbTransceiver::captureOnce(IrSignal& outSignal, float timeoutSeconds) {
    stopBackgroundPolling();

    std::lock_guard<std::mutex> lock(m_usbMutex);
    if (!m_devHandle) return false;

    flushInputBuffer();

    setState(DeviceState::Receiving);

    sendCommand('R');
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::vector<uint8_t> dummy;
    readData(dummy, 300);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<float>(timeoutSeconds);
    bool captured = false;

    while (std::chrono::steady_clock::now() < deadline) {
        sendCommand('O');
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::vector<uint8_t> payload;
        if (readPayload(payload, 800)) {
            if (IrProtocol::decodeTiqiaaPayload(payload, outSignal)) {
                captured = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    sendCommand('L');
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    readData(dummy, 300);

    setState(DeviceState::Idle);
    return captured;
}

void IrUsbTransceiver::startBackgroundPolling() {
    if (m_pollingActive) return;
    if (!m_devHandle) return;

    m_pollingActive = true;
    m_pollingThread = std::thread(&IrUsbTransceiver::pollingLoop, this);
    log("Started background polling thread");
}

void IrUsbTransceiver::stopBackgroundPolling() {
    if (!m_pollingActive) return;

    m_pollingActive = false;
    if (m_pollingThread.joinable()) {
        m_pollingThread.join();
    }
    log("Stopped background polling thread");
}

void IrUsbTransceiver::pollingLoop() {
    {
        std::lock_guard<std::mutex> lock(m_usbMutex);
        if (!m_devHandle) {
            m_pollingActive = false;
            return;
        }
        flushInputBuffer();
        setState(DeviceState::Receiving);
        sendCommand('R');
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::vector<uint8_t> dummy;
        readData(dummy, 300);
    }

    while (m_pollingActive) {
        IrSignal sig;
        bool gotSignal = false;

        {
            std::lock_guard<std::mutex> lock(m_usbMutex);
            if (!m_devHandle) break;

            sendCommand('O');
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            std::vector<uint8_t> payload;
            if (readPayload(payload, 800)) {
                if (IrProtocol::decodeTiqiaaPayload(payload, sig)) {
                    gotSignal = true;
                }
            }
        }

        if (gotSignal) {
            m_rxRingBuffer.push(sig);
            if (m_onSignalReceived) {
                m_onSignalReceived(sig);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    {
        std::lock_guard<std::mutex> lock(m_usbMutex);
        if (m_devHandle) {
            sendCommand('L');
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<uint8_t> dummy;
            readData(dummy, 300);
        }
        setState(DeviceState::Idle);
    }
}

std::optional<IrSignal> IrUsbTransceiver::popReceivedSignal() {
    return m_rxRingBuffer.pop();
}

void IrUsbTransceiver::setSignalReceivedCallback(SignalReceivedCallback cb) {
    m_onSignalReceived = cb;
}

void IrUsbTransceiver::setStateChangedCallback(StateChangedCallback cb) {
    m_onStateChanged = cb;
}

void IrUsbTransceiver::setLogCallback(LogCallback cb) {
    m_onLog = cb;
}

} // namespace ir
