#pragma once

#include "IrUsbTransceiver.h"
#include "IrProtocol.h"
#include "SignalStorage.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <optional>

namespace ir {

/**
 * @brief Configuration parameters for the HTTP REST API server.
 */
struct HttpServerConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::string storagePath{""};
    bool enableCors{true};
    bool autoStartPolling{true};
};

/**
 * @brief Embedded REST HTTP API Server for the IR Transceiver application.
 * Exposes endpoints for querying transceiver status, transmitting IR pulses,
 * reading captured signals from the ring buffer, and managing stored IR signals.
 */
class HttpServerModule {
public:
    explicit HttpServerModule(std::shared_ptr<IrUsbTransceiver> transceiver,
                              std::shared_ptr<SignalStorage> storage = nullptr,
                              HttpServerConfig config = {});
    ~HttpServerModule();

    HttpServerModule(const HttpServerModule&) = delete;
    HttpServerModule& operator=(const HttpServerModule&) = delete;

    /**
     * @brief Starts the HTTP server asynchronously on a dedicated worker thread.
     * @return true if the server thread was successfully launched, false if already running or failed.
     */
    bool start();

    /**
     * @brief Stops the HTTP server and joins the listener thread.
     */
    void stop();

    /**
     * @brief Blocks current thread until the HTTP server terminates.
     */
    void wait();

    /**
     * @brief Returns true if the HTTP server is currently listening.
     */
    bool isRunning() const;

    int getPort() const { return m_config.port; }
    const std::string& getHost() const { return m_config.host; }

    std::shared_ptr<IrUsbTransceiver> getTransceiver() const { return m_transceiver; }
    std::shared_ptr<SignalStorage> getStorage() const { return m_storage; }

    /**
     * @brief Returns reference to underlying httplib::Server.
     */
    httplib::Server& getNativeServer() { return m_server; }

    /**
     * @brief Helper to convert IrSignal to JSON representation.
     */
    static nlohmann::json signalToJson(const IrSignal& sig, const SignalStorage* storage = nullptr);

private:
    void setupRoutes();
    void setCorsHeaders(httplib::Response& res);

    // Route handlers
    void handleRoot(const httplib::Request& req, httplib::Response& res);
    void handleGetStatus(const httplib::Request& req, httplib::Response& res);
    void handlePostSend(const httplib::Request& req, httplib::Response& res);
    void handleGetReceive(const httplib::Request& req, httplib::Response& res);
    void handlePostCapture(const httplib::Request& req, httplib::Response& res);
    void handlePostMode(const httplib::Request& req, httplib::Response& res);
    void handleGetSignals(const httplib::Request& req, httplib::Response& res);
    void handleGetSignalByIndex(const httplib::Request& req, httplib::Response& res);
    void handlePostSignals(const httplib::Request& req, httplib::Response& res);
    void handleDeleteSignalByIndex(const httplib::Request& req, httplib::Response& res);
    void handlePostSendSavedSignal(const httplib::Request& req, httplib::Response& res);

    std::shared_ptr<IrUsbTransceiver> m_transceiver;
    std::shared_ptr<SignalStorage> m_storage;
    HttpServerConfig m_config;

    httplib::Server m_server;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;

    mutable std::mutex m_lastSignalMutex;
    std::optional<IrSignal> m_lastCapturedSignal;
};

} // namespace ir
