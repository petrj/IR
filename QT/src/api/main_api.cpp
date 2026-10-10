#include "HttpServerModule.h"
#include "IrUsbTransceiver.h"
#include "SignalStorage.h"

#include <iostream>
#include <string>
#include <csignal>
#include <memory>
#include <atomic>

static std::atomic<bool> g_shutdownRequested{false};
static ir::HttpServerModule* g_serverInstance{nullptr};

static void signalHandler(int signum) {
    std::cout << "\n[ir_api_server] Received signal " << signum << ", shutting down gracefully...\n";
    g_shutdownRequested.store(true);
    if (g_serverInstance) {
        g_serverInstance->stop();
    }
}

static void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>        HTTP listen port (default: 8080)\n"
              << "  -h, --host <host>        HTTP listen address (default: 0.0.0.0)\n"
              << "  -s, --storage <path>     Path to saved IR signals JSON file\n"
              << "  --help                   Display this help message\n\n"
              << "Examples:\n"
              << "  " << progName << " --port 8080\n"
              << "  " << progName << " --host 127.0.0.1 --port 9090\n";
}

int main(int argc, char* argv[]) {
    ir::HttpServerConfig config;
    config.port = 8080;
    config.host = "0.0.0.0";
    config.enableCors = true;
    config.autoStartPolling = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-s" || arg == "--storage") && i + 1 < argc) {
            config.storagePath = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "========================================================\n";
    std::cout << " Multi-Platform IR Transceiver REST API Server\n";
    std::cout << "========================================================\n";

    // Initialize hardware transceiver
    auto transceiver = std::make_shared<ir::IrUsbTransceiver>(38);
    bool usbConnected = transceiver->openDevice();

    if (usbConnected) {
        auto devInfo = transceiver->getDeviceInfo();
        std::cout << "[USB] Successfully opened IR Transceiver:\n";
        std::cout << "      Manufacturer: " << devInfo.manufacturer << "\n";
        std::cout << "      Product:      " << devInfo.product << "\n";
        std::cout << "      Firmware:     " << transceiver->getFirmwareVersion() << "\n";
    } else {
        std::cout << "[USB] Notice: IR Transceiver hardware not detected on USB.\n";
        std::cout << "      The REST API will remain active for status checks & signal management.\n";
        std::cout << "      (Ensure udev rules are installed on Linux: see project_readme.md)\n";
    }

    auto storage = std::make_shared<ir::SignalStorage>(config.storagePath);
    std::cout << "[Storage] Loaded " << storage->count() << " saved signals from "
              << storage->getStoragePath() << "\n";

    ir::HttpServerModule server(transceiver, storage, config);
    g_serverInstance = &server;

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[HTTP] Starting REST server on http://" << config.host << ":" << config.port << " ...\n";
    if (!server.start()) {
        std::cerr << "[HTTP] Fatal error: Failed to start HTTP server!\n";
        return 1;
    }

    std::cout << "[HTTP] Listening on http://" << config.host << ":" << config.port << "\n";
    std::cout << "       Endpoints:\n";
    std::cout << "         GET    /api/v1/status\n";
    std::cout << "         POST   /api/v1/send\n";
    std::cout << "         GET    /api/v1/receive\n";
    std::cout << "         POST   /api/v1/capture\n";
    std::cout << "         GET    /api/v1/signals\n";
    std::cout << "\nTry querying status via:\n";
    std::cout << "  curl -s http://127.0.0.1:" << config.port << "/api/v1/status\n\n";
    std::cout << "Press Ctrl+C to terminate server.\n";

    // Wait until stopped
    server.wait();

    std::cout << "[ir_api_server] Server stopped. Exiting.\n";
    return 0;
}
