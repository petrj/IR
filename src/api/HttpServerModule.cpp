#include "HttpServerModule.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace ir {

HttpServerModule::HttpServerModule(std::shared_ptr<IrUsbTransceiver> transceiver,
                                   std::shared_ptr<SignalStorage> storage,
                                   HttpServerConfig config)
    : m_transceiver(std::move(transceiver))
    , m_storage(std::move(storage))
    , m_config(std::move(config)) {

    if (!m_storage) {
        m_storage = std::make_shared<SignalStorage>(m_config.storagePath);
    }

    if (m_transceiver) {
        m_transceiver->setSignalReceivedCallback([this](const IrSignal& sig) {
            std::lock_guard<std::mutex> lock(m_lastSignalMutex);
            m_lastCapturedSignal = sig;
        });
    }

    setupRoutes();
}

HttpServerModule::~HttpServerModule() {
    stop();
}

void HttpServerModule::setCorsHeaders(httplib::Response& res) {
    if (m_config.enableCors) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
    }
}

void HttpServerModule::setupRoutes() {
    // CORS preflight handler
    if (m_config.enableCors) {
        m_server.Options(".*", [this](const httplib::Request&, httplib::Response& res) {
            setCorsHeaders(res);
            res.status = 204;
        });
    }

    // Root info endpoint
    m_server.Get("/", [this](const httplib::Request& req, httplib::Response& res) {
        handleRoot(req, res);
    });

    // Device status endpoint
    m_server.Get("/api/v1/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetStatus(req, res);
    });

    // IR Transmit endpoint
    m_server.Post("/api/v1/send", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostSend(req, res);
    });

    // Receive buffer read endpoint
    m_server.Get("/api/v1/receive", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetReceive(req, res);
    });

    // Single-shot capture endpoint (blocking with timeout)
    m_server.Post("/api/v1/capture", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostCapture(req, res);
    });

    // Hardware mode control
    m_server.Post("/api/v1/mode", [this](const httplib::Request& req, httplib::Response& res) {
        handlePostMode(req, res);
    });

}

bool HttpServerModule::start() {
    if (m_running.load()) {
        return false;
    }

    if (m_config.autoStartPolling && m_transceiver && m_transceiver->isConnected()) {
        m_transceiver->startBackgroundPolling();
    }

    m_running.store(true);
    m_serverThread = std::thread([this]() {
        bool ok = m_server.listen(m_config.host, m_config.port);
        m_running.store(false);
        if (!ok) {
            std::cerr << "[HttpServerModule] Error: Server failed to listen on "
                      << m_config.host << ":" << m_config.port << std::endl;
        }
    });

    // Give server listener thread a brief moment to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

void HttpServerModule::stop() {
    if (m_running.load()) {
        m_server.stop();
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
        m_running.store(false);
    }
    if (m_transceiver) {
        m_transceiver->stopBackgroundPolling();
    }
}

void HttpServerModule::wait() {
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

bool HttpServerModule::isRunning() const {
    return m_running.load();
}

nlohmann::json HttpServerModule::signalToJson(const IrSignal& sig, const SignalStorage* storage) {
    nlohmann::json out;
    out["frequency"] = static_cast<int>(sig.frequency_khz);
    out["pulseCount"] = sig.pulses.size();
    out["pulses"] = sig.pulses;

    auto timeT = std::chrono::system_clock::to_time_t(sig.timestamp);
    std::tm tmBuf{};
    localtime_r(&timeT, &tmBuf);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    out["timestamp"] = std::string(buf);

    DecodedCode decoded;
    if (IrProtocol::decodeNec(sig.pulses, decoded)) {
        std::ostringstream ssHex, ssAddr, ssCmd;
        ssHex << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << decoded.rawValue;
        ssAddr << "0x" << std::uppercase << std::hex << (decoded.address > 0xFF ? std::setw(4) : std::setw(2)) << std::setfill('0') << decoded.address;
        ssCmd << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << decoded.command;

        out["decoded"] = {
            {"protocol", decoded.protocolName()},
            {"rawValue", ssHex.str()},
            {"address", ssAddr.str()},
            {"command", ssCmd.str()},
            {"isRepeat", decoded.isRepeat}
        };
    }

    if (storage) {
        auto match = storage->findMatchingSignal(sig);
        if (match) {
            out["matchedSavedSignal"] = match->name;
            out["matchedRcdName"] = match->rcdName;
        }
    }

    return out;
}

void HttpServerModule::handleRoot(const httplib::Request&, httplib::Response& res) {
    setCorsHeaders(res);
    nlohmann::json j = {
        {"service", "Multi-Platform IR Transceiver REST API"},
        {"version", "1.0.0"},
        {"documentation", "/api/v1/status"},
        {"endpoints", {
            {"GET /api/v1/status", "Returns transceiver connection & hardware state"},
            {"POST /api/v1/send", "Transmits an IR signal (NEC code or Raw pulse array)"},
            {"GET /api/v1/receive", "Pops oldest signal from receive buffer (supports ?last=true query)"},
            {"POST /api/v1/capture", "Performs synchronous single-shot capture with timeout"},
            {"POST /api/v1/mode", "Changes transceiver mode (idle, receive, start_polling, stop_polling)"}
        }}
    };
    res.set_content(j.dump(2), "application/json");
}

void HttpServerModule::handleGetStatus(const httplib::Request&, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver) {
        res.status = 503;
        nlohmann::json err = {{"connected", false}, {"error", "Transceiver driver not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    try {
        DeviceInfo info = m_transceiver->getDeviceInfo();
        DeviceState st = m_transceiver->getState();
        std::string stateStr = "Disconnected";
        switch (st) {
            case DeviceState::Disconnected: stateStr = "Disconnected"; break;
            case DeviceState::Idle: stateStr = "Idle"; break;
            case DeviceState::Receiving: stateStr = "Receiving"; break;
            case DeviceState::Transmitting: stateStr = "Transmitting"; break;
            case DeviceState::Error: stateStr = "Error"; break;
        }

        auto sanitizeAscii = [](const std::string& str) -> std::string {
            std::string out;
            for (char c : str) {
                if (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) <= 126) {
                    out.push_back(c);
                }
            }
            return out;
        };

        std::ostringstream vidSs, pidSs;
        vidSs << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << info.vendorId;
        pidSs << "0x" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << info.productId;

        std::string fw = "N/A";
        if (m_transceiver->isConnected()) {
            fw = sanitizeAscii(m_transceiver->getFirmwareVersion());
            if (fw.empty()) fw = "Unknown";
        }

        nlohmann::json j;
        j["connected"] = m_transceiver->isConnected();
        j["state"] = stateStr;
        j["device"] = {
            {"vendorId", vidSs.str()},
            {"productId", pidSs.str()},
            {"manufacturer", sanitizeAscii(info.manufacturer)},
            {"product", sanitizeAscii(info.product)},
            {"serialNumber", sanitizeAscii(info.serialNumber)},
            {"defaultFrequency", static_cast<int>(info.defaultFrequency)},
            {"firmwareVersion", fw}
        };

        res.status = 200;
        res.set_content(j.dump(2), "application/json");
    } catch (const std::exception& e) {
        res.status = 200;
        nlohmann::json fallback = {
            {"connected", m_transceiver ? m_transceiver->isConnected() : false},
            {"state", "Connected"}
        };
        res.set_content(fallback.dump(2), "application/json");
    }
}

void HttpServerModule::handlePostSend(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver || !m_transceiver->isConnected()) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "USB Transceiver is not connected"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(req.body);
    } catch (const std::exception& e) {
        res.status = 400;
        nlohmann::json err = {{"success", false}, {"error", std::string("Malformed JSON body: ") + e.what()}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    // 1. Transmit by saved signal name
    if (j.contains("name") && j["name"].is_string()) {
        std::string name = j["name"].get<std::string>();
        if (!m_storage) {
            res.status = 400;
            nlohmann::json err = {{"success", false}, {"error", "Signal storage is not available"}};
            res.set_content(err.dump(2), "application/json");
            return;
        }

        bool found = false;
        for (const auto& sig : m_storage->getSignals()) {
            if (sig.name == name) {
                found = true;
                uint8_t freq = static_cast<uint8_t>(j.value("frequency", static_cast<int>(sig.frequency_khz)));
                bool ok = m_transceiver->transmitIr(sig.pulses, freq);
                if (ok) {
                    res.status = 200;
                    nlohmann::json out = {
                        {"success", true},
                        {"message", "Transmitted stored signal '" + name + "'"},
                        {"name", name},
                        {"protocol", sig.protocol},
                        {"pulseCount", sig.pulses.size()},
                        {"frequency", static_cast<int>(freq)}
                    };
                    res.set_content(out.dump(2), "application/json");
                } else {
                    res.status = 500;
                    nlohmann::json err = {{"success", false}, {"error", "Hardware failed to transmit signal"}};
                    res.set_content(err.dump(2), "application/json");
                }
                return;
            }
        }

        if (!found) {
            res.status = 404;
            nlohmann::json err = {{"success", false}, {"error", "Stored signal not found: " + name}};
            res.set_content(err.dump(2), "application/json");
            return;
        }
    }

    std::string protocol = j.value("protocol", "NEC");
    uint8_t frequency = static_cast<uint8_t>(j.value("frequency", 38));

    // 2. NEC Protocol transmission
    if (protocol == "NEC" || protocol == "nec") {
        if (j.contains("code")) {
            uint32_t rawValue = 0;
            if (j["code"].is_number()) {
                rawValue = j["code"].get<uint32_t>();
            } else if (j["code"].is_string()) {
                std::string codeStr = j["code"].get<std::string>();
                try {
                    rawValue = static_cast<uint32_t>(std::stoul(codeStr, nullptr, 0));
                } catch (...) {
                    res.status = 400;
                    nlohmann::json err = {{"success", false}, {"error", "Invalid hex/integer format in 'code'"}};
                    res.set_content(err.dump(2), "application/json");
                    return;
                }
            }

            // Encode 32-bit NEC sequence
            std::vector<int> pulses;
            pulses.reserve(68);
            pulses.push_back(9000);
            pulses.push_back(-4500);
            for (int i = 0; i < 32; ++i) {
                pulses.push_back(560);
                pulses.push_back(((rawValue >> i) & 1) ? -1690 : -560);
            }
            pulses.push_back(560);
            pulses.push_back(-40000);

            int repeats = j.value("repeats", 0);
            for (int r = 0; r < repeats; ++r) {
                pulses.push_back(9000);
                pulses.push_back(-2250);
                pulses.push_back(560);
                pulses.push_back(-40000);
            }

            bool ok = m_transceiver->transmitIr(pulses, frequency);
            if (ok) {
                std::ostringstream ss;
                ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << rawValue;
                res.status = 200;
                nlohmann::json out = {
                    {"success", true},
                    {"message", "NEC signal transmitted successfully"},
                    {"protocol", "NEC"},
                    {"code", ss.str()},
                    {"pulseCount", pulses.size()},
                    {"frequency", static_cast<int>(frequency)}
                };
                res.set_content(out.dump(2), "application/json");
            } else {
                res.status = 500;
                nlohmann::json err = {{"success", false}, {"error", "Hardware failed to transmit signal"}};
                res.set_content(err.dump(2), "application/json");
            }
            return;
        } else if (j.contains("address") && j.contains("command")) {
            uint32_t addr = 0;
            uint32_t cmd = 0;
            try {
                if (j["address"].is_number()) {
                    addr = j["address"].get<uint32_t>();
                } else if (j["address"].is_string()) {
                    std::string addrStr = j["address"].get<std::string>();
                    addr = static_cast<uint32_t>(std::stoul(addrStr, nullptr, 0));
                    // If user specified "0x00XX" with 4 hex digits (e.g., Hisense "0x00BF"), map to 16-bit extended address
                    if ((addrStr.rfind("0x00", 0) == 0 || addrStr.rfind("0X00", 0) == 0) && addrStr.length() == 6) {
                        uint32_t byte1 = static_cast<uint32_t>(std::stoul(addrStr.substr(4), nullptr, 16));
                        addr = (byte1 << 8) | 0x00;
                    }
                }

                if (j["command"].is_number()) cmd = j["command"].get<uint32_t>();
                else if (j["command"].is_string()) cmd = static_cast<uint32_t>(std::stoul(j["command"].get<std::string>(), nullptr, 0));
            } catch (...) {
                res.status = 400;
                nlohmann::json err = {{"success", false}, {"error", "Invalid format for 'address' or 'command'"}};
                res.set_content(err.dump(2), "application/json");
                return;
            }

            std::vector<int> pulses = IrProtocol::encodeNec(addr, cmd);
            int repeats = j.value("repeats", 0);
            for (int r = 0; r < repeats; ++r) {
                pulses.push_back(9000);
                pulses.push_back(-2250);
                pulses.push_back(560);
                pulses.push_back(-40000);
            }

            bool ok = m_transceiver->transmitIr(pulses, frequency);
            if (ok) {
                res.status = 200;
                nlohmann::json out = {
                    {"success", true},
                    {"message", "NEC signal transmitted successfully"},
                    {"protocol", "NEC"},
                    {"address", addr},
                    {"command", cmd},
                    {"pulseCount", pulses.size()},
                    {"frequency", static_cast<int>(frequency)}
                };
                res.set_content(out.dump(2), "application/json");
            } else {
                res.status = 500;
                nlohmann::json err = {{"success", false}, {"error", "Hardware failed to transmit signal"}};
                res.set_content(err.dump(2), "application/json");
            }
            return;
        } else {
            res.status = 400;
            nlohmann::json err = {{"success", false}, {"error", "NEC protocol requires 'code' or ('address' and 'command')"}};
            res.set_content(err.dump(2), "application/json");
            return;
        }
    }

    // 3. Raw pulse sequence transmission
    if (protocol == "Raw" || protocol == "raw") {
        if (!j.contains("pulses") || !j["pulses"].is_array()) {
            res.status = 400;
            nlohmann::json err = {{"success", false}, {"error", "Raw protocol requires 'pulses' array of microsecond durations"}};
            res.set_content(err.dump(2), "application/json");
            return;
        }

        std::vector<int> pulses = j["pulses"].get<std::vector<int>>();
        if (pulses.empty()) {
            res.status = 400;
            nlohmann::json err = {{"success", false}, {"error", "'pulses' array cannot be empty"}};
            res.set_content(err.dump(2), "application/json");
            return;
        }

        bool ok = m_transceiver->transmitIr(pulses, frequency);
        if (ok) {
            res.status = 200;
            nlohmann::json out = {
                {"success", true},
                {"message", "Raw IR signal transmitted successfully"},
                {"protocol", "Raw"},
                {"pulseCount", pulses.size()},
                {"frequency", static_cast<int>(frequency)}
            };
            res.set_content(out.dump(2), "application/json");
        } else {
            res.status = 500;
            nlohmann::json err = {{"success", false}, {"error", "Hardware failed to transmit signal"}};
            res.set_content(err.dump(2), "application/json");
        }
        return;
    }

    res.status = 400;
    nlohmann::json err = {{"success", false}, {"error", "Unsupported protocol: " + protocol + " (supported: 'NEC', 'Raw')"}};
    res.set_content(err.dump(2), "application/json");
}

void HttpServerModule::handleGetReceive(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver) {
        res.status = 503;
        nlohmann::json err = {{"available", false}, {"error", "Transceiver driver not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    std::optional<IrSignal> sigOpt = m_transceiver->popReceivedSignal();

    // If queue is empty and caller asked for ?last=true, return last cached signal
    if (!sigOpt && req.has_param("last") && req.get_param_value("last") == "true") {
        std::lock_guard<std::mutex> lock(m_lastSignalMutex);
        sigOpt = m_lastCapturedSignal;
    }

    if (sigOpt) {
        res.status = 200;
        nlohmann::json out = {
            {"available", true},
            {"signal", signalToJson(*sigOpt, m_storage.get())}
        };
        res.set_content(out.dump(2), "application/json");
    } else {
        res.status = 200;
        nlohmann::json out = {
            {"available", false},
            {"message", "Receive buffer is empty"}
        };
        res.set_content(out.dump(2), "application/json");
    }
}

void HttpServerModule::handlePostCapture(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver || !m_transceiver->isConnected()) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "USB Transceiver is not connected"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    float timeoutSec = 5.0f;
    if (req.has_param("timeout")) {
        try {
            timeoutSec = std::stof(req.get_param_value("timeout"));
        } catch (...) {}
    } else if (!req.body.empty()) {
        try {
            auto j = nlohmann::json::parse(req.body);
            if (j.contains("timeout") && j["timeout"].is_number()) {
                timeoutSec = j["timeout"].get<float>();
            }
        } catch (...) {}
    }

    IrSignal sig;
    bool ok = m_transceiver->captureOnce(sig, timeoutSec);
    if (ok) {
        {
            std::lock_guard<std::mutex> lock(m_lastSignalMutex);
            m_lastCapturedSignal = sig;
        }
        res.status = 200;
        nlohmann::json out = {
            {"success", true},
            {"signal", signalToJson(sig, m_storage.get())}
        };
        res.set_content(out.dump(2), "application/json");
    } else {
        res.status = 408; // Request Timeout
        nlohmann::json out = {
            {"success", false},
            {"error", "IR signal capture timed out after " + std::to_string(timeoutSec) + " seconds"}
        };
        res.set_content(out.dump(2), "application/json");
    }
}

void HttpServerModule::handlePostMode(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver || !m_transceiver->isConnected()) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "USB Transceiver is not connected"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(req.body);
    } catch (const std::exception& e) {
        res.status = 400;
        nlohmann::json err = {{"success", false}, {"error", std::string("Malformed JSON: ") + e.what()}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    std::string mode = j.value("mode", "");
    bool ok = false;
    if (mode == "idle") {
        ok = m_transceiver->setIdle();
    } else if (mode == "receive") {
        ok = m_transceiver->startReceive();
    } else if (mode == "start_polling") {
        m_transceiver->startBackgroundPolling();
        ok = true;
    } else if (mode == "stop_polling") {
        m_transceiver->stopBackgroundPolling();
        ok = true;
    } else {
        res.status = 400;
        nlohmann::json err = {{"success", false}, {"error", "Invalid mode: " + mode + " (allowed: 'idle', 'receive', 'start_polling', 'stop_polling')"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    res.status = ok ? 200 : 500;
    nlohmann::json out = {
        {"success", ok},
        {"mode", mode}
    };
    res.set_content(out.dump(2), "application/json");
}

void HttpServerModule::handleGetSignals(const httplib::Request&, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_storage) {
        res.status = 503;
        nlohmann::json err = {{"error", "Signal storage is not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    nlohmann::json list = nlohmann::json::array();
    const auto& sigs = m_storage->getSignals();
    for (size_t i = 0; i < sigs.size(); ++i) {
        const auto& s = sigs[i];
        nlohmann::json item = {
            {"index", i},
            {"name", s.name},
            {"rcdName", s.rcdName},
            {"protocol", s.protocol},
            {"frequency", static_cast<int>(s.frequency_khz)},
            {"timestamp", s.timestamp},
            {"rawHex", s.rawHex},
            {"pulseCount", s.pulses.size()}
        };
        list.push_back(item);
    }

    nlohmann::json out = {
        {"count", sigs.size()},
        {"storagePath", m_storage->getStoragePath()},
        {"signals", list}
    };
    res.status = 200;
    res.set_content(out.dump(2), "application/json");
}

void HttpServerModule::handleGetSignalByIndex(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_storage) {
        res.status = 503;
        nlohmann::json err = {{"error", "Signal storage is not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    size_t index = std::stoul(req.matches[1]);
    auto sigOpt = m_storage->getSignal(index);
    if (!sigOpt) {
        res.status = 404;
        nlohmann::json err = {{"error", "Signal not found at index " + std::to_string(index)}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    const auto& s = *sigOpt;
    nlohmann::json out = {
        {"index", index},
        {"name", s.name},
        {"rcdName", s.rcdName},
        {"protocol", s.protocol},
        {"frequency", static_cast<int>(s.frequency_khz)},
        {"timestamp", s.timestamp},
        {"rawHex", s.rawHex},
        {"pulseCount", s.pulses.size()},
        {"pulses", s.pulses}
    };
    res.status = 200;
    res.set_content(out.dump(2), "application/json");
}

void HttpServerModule::handlePostSignals(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_storage) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "Signal storage is not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(req.body);
    } catch (const std::exception& e) {
        res.status = 400;
        nlohmann::json err = {{"success", false}, {"error", std::string("Malformed JSON: ") + e.what()}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    SavedIrSignal sig;
    sig.name = j.value("name", "Unnamed Signal");
    sig.rcdName = j.value("rcdName", j.value("rcd", j.value("device", "Default Remote Device")));
    sig.protocol = j.value("protocol", "Raw");
    sig.frequency_khz = static_cast<uint8_t>(j.value("frequency", 38));
    sig.rawHex = j.value("rawHex", "N/A");

    if (j.contains("timestamp") && j["timestamp"].is_string()) {
        sig.timestamp = j["timestamp"].get<std::string>();
    } else {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf{};
        localtime_r(&timeT, &tmBuf);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
        sig.timestamp = std::string(buf);
    }

    if (j.contains("pulses") && j["pulses"].is_array()) {
        sig.pulses = j["pulses"].get<std::vector<int>>();
    } else if (sig.protocol == "NEC" && sig.rawHex != "N/A") {
        try {
            uint32_t rawVal = static_cast<uint32_t>(std::stoul(sig.rawHex, nullptr, 0));
            sig.pulses.reserve(68);
            sig.pulses.push_back(9000);
            sig.pulses.push_back(-4500);
            for (int i = 0; i < 32; ++i) {
                sig.pulses.push_back(560);
                sig.pulses.push_back(((rawVal >> i) & 1) ? -1690 : -560);
            }
            sig.pulses.push_back(560);
            sig.pulses.push_back(-40000);
        } catch (...) {}
    }

    if (sig.pulses.empty()) {
        res.status = 400;
        nlohmann::json err = {{"success", false}, {"error", "Signal requires 'pulses' array or valid NEC 'rawHex'"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    m_storage->addSignal(sig, sig.rcdName);
    size_t newIndex = m_storage->count() - 1;

    res.status = 201;
    nlohmann::json out = {
        {"success", true},
        {"message", "Saved signal successfully"},
        {"index", newIndex},
        {"name", sig.name}
    };
    res.set_content(out.dump(2), "application/json");
}

void HttpServerModule::handleDeleteSignalByIndex(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_storage) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "Signal storage is not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    size_t index = std::stoul(req.matches[1]);
    bool ok = m_storage->removeSignal(index);
    if (ok) {
        res.status = 200;
        nlohmann::json out = {
            {"success", true},
            {"message", "Deleted signal at index " + std::to_string(index)}
        };
        res.set_content(out.dump(2), "application/json");
    } else {
        res.status = 404;
        nlohmann::json err = {
            {"success", false},
            {"error", "Signal index out of range: " + std::to_string(index)}
        };
        res.set_content(err.dump(2), "application/json");
    }
}

void HttpServerModule::handlePostSendSavedSignal(const httplib::Request& req, httplib::Response& res) {
    setCorsHeaders(res);

    if (!m_transceiver || !m_transceiver->isConnected()) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "USB Transceiver is not connected"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    if (!m_storage) {
        res.status = 503;
        nlohmann::json err = {{"success", false}, {"error", "Signal storage is not initialized"}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    size_t index = std::stoul(req.matches[1]);
    auto sigOpt = m_storage->getSignal(index);
    if (!sigOpt) {
        res.status = 404;
        nlohmann::json err = {{"success", false}, {"error", "Signal not found at index " + std::to_string(index)}};
        res.set_content(err.dump(2), "application/json");
        return;
    }

    bool ok = m_transceiver->transmitIr(sigOpt->pulses, sigOpt->frequency_khz);
    if (ok) {
        res.status = 200;
        nlohmann::json out = {
            {"success", true},
            {"message", "Transmitted saved signal '" + sigOpt->name + "'"},
            {"index", index},
            {"name", sigOpt->name},
            {"pulseCount", sigOpt->pulses.size()}
        };
        res.set_content(out.dump(2), "application/json");
    } else {
        res.status = 500;
        nlohmann::json err = {{"success", false}, {"error", "Hardware failed to transmit signal"}};
        res.set_content(err.dump(2), "application/json");
    }
}

} // namespace ir
