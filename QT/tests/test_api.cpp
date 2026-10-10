#include "HttpServerModule.h"
#include "IrUsbTransceiver.h"
#include "SignalStorage.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <cassert>
#include <memory>
#include <cstdio>

void testApiServerEndpoints() {
    std::cout << "[TEST] Starting HTTP REST API Server integration tests..." << std::endl;

    const std::string testStoragePath = "/tmp/test_api_storage.json";
    std::remove(testStoragePath.c_str());

    auto transceiver = std::make_shared<ir::IrUsbTransceiver>(38);
    auto storage = std::make_shared<ir::SignalStorage>(testStoragePath);

    // Populate an initial stored signal
    ir::SavedIrSignal sample;
    sample.name = "Test TV Mute";
    sample.protocol = "NEC";
    sample.rawHex = "0xE718FF00";
    sample.frequency_khz = 38;
    sample.pulses = ir::IrProtocol::encodeNec(0x00, 0x18);
    sample.timestamp = "2026-10-10 12:00:00";
    storage->addSignal(sample);
    assert(storage->count() == 1);

    ir::HttpServerConfig config;
    config.host = "127.0.0.1";
    config.port = 18080;
    config.enableCors = true;
    config.autoStartPolling = false; // Headless test environment

    ir::HttpServerModule server(transceiver, storage, config);
    bool started = server.start();
    assert(started);
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", 18080);
    client.set_connection_timeout(2);
    client.set_read_timeout(2);

    // 1. Test GET / (Root info)
    {
        std::cout << "  Testing GET / ..." << std::endl;
        auto res = client.Get("/");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("service"));
        assert(j.contains("version"));
        assert(j.contains("endpoints"));
        assert(res->has_header("Access-Control-Allow-Origin"));
    }

    // 2. Test GET /api/v1/status
    {
        std::cout << "  Testing GET /api/v1/status ..." << std::endl;
        auto res = client.Get("/api/v1/status");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("connected"));
        assert(j.contains("state"));
        assert(j.contains("device"));
        assert(j["device"].contains("vendorId"));
        assert(j["device"].contains("productId"));
    }

    // 3. Test GET /api/v1/receive when buffer is empty
    {
        std::cout << "  Testing GET /api/v1/receive ..." << std::endl;
        auto res = client.Get("/api/v1/receive");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("available"));
        assert(j["available"] == false);
    }

    // 4. Test GET /api/v1/signals
    {
        std::cout << "  Testing GET /api/v1/signals ..." << std::endl;
        auto res = client.Get("/api/v1/signals");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("count"));
        assert(j["count"] == 1);
        assert(j["signals"].is_array());
        assert(j["signals"][0]["name"] == "Test TV Mute");
    }

    // 5. Test GET /api/v1/signals/0
    {
        std::cout << "  Testing GET /api/v1/signals/0 ..." << std::endl;
        auto res = client.Get("/api/v1/signals/0");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j["name"] == "Test TV Mute");
        assert(j.contains("pulses"));
    }

    // 6. Test POST /api/v1/signals (Add signal)
    {
        std::cout << "  Testing POST /api/v1/signals ..." << std::endl;
        nlohmann::json newSig = {
            {"name", "Test Power Button"},
            {"protocol", "NEC"},
            {"rawHex", "0xA55AFF00"},
            {"frequency", 38}
        };
        auto res = client.Post("/api/v1/signals", newSig.dump(), "application/json");
        assert(res && res->status == 201);
        auto j = nlohmann::json::parse(res->body);
        assert(j["success"] == true);
        assert(j["name"] == "Test Power Button");
        assert(storage->count() == 2);
    }

    // 7. Test DELETE /api/v1/signals/1
    {
        std::cout << "  Testing DELETE /api/v1/signals/1 ..." << std::endl;
        auto res = client.Delete("/api/v1/signals/1");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j["success"] == true);
        assert(storage->count() == 1);
    }

    // 8. Test POST /api/v1/send validation errors
    {
        std::cout << "  Testing POST /api/v1/send error handling..." << std::endl;

        // Malformed JSON
        auto res1 = client.Post("/api/v1/send", "not-a-json", "application/json");
        // Should be 400 or 503 (if device disconnected check precedes)
        assert(res1 && (res1->status == 400 || res1->status == 503));

        // When device is disconnected, sending valid payload returns 503
        nlohmann::json validPayload = {
            {"protocol", "NEC"},
            {"code", "0x00FF45BA"}
        };
        auto res2 = client.Post("/api/v1/send", validPayload.dump(), "application/json");
        assert(res2);
        if (!transceiver->isConnected()) {
            assert(res2->status == 503);
            auto j2 = nlohmann::json::parse(res2->body);
            assert(j2["success"] == false);
        }
    }

    // 9. Stop server
    std::cout << "  Stopping server..." << std::endl;
    server.stop();
    assert(!server.isRunning());

    std::remove(testStoragePath.c_str());
    std::cout << "  PASSED: HTTP REST API integration tests" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << " Running IR REST API Unit & Integration Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    testApiServerEndpoints();

    std::cout << "==========================================" << std::endl;
    std::cout << " ALL REST API TESTS PASSED SUCCESSFULLY! " << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
