#include "RingBuffer.h"
#include "IrProtocol.h"
#include "IrUsbTransceiver.h"
#include "SignalStorage.h"
#include "HttpServerModule.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <memory>

// ============================================================================
// PART 1: IR CORE ENGINE UNIT TESTS
// ============================================================================

void testRingBuffer() {
    std::cout << "  [CORE] Testing RingBuffer FIFO and overwrite..." << std::endl;
    ir::RingBuffer<int, 4> buffer;

    assert(buffer.capacity() == 4);
    assert(buffer.empty());
    assert(buffer.size() == 0);

    buffer.push(10);
    buffer.push(20);
    buffer.push(30);

    assert(buffer.size() == 3);
    assert(!buffer.empty());
    assert(!buffer.full());

    buffer.push(40);
    assert(buffer.full());

    // Push when full overwrites oldest (10)
    buffer.push(50);
    assert(buffer.full());

    int val = 0;
    assert(buffer.pop(val) && val == 20);
    assert(buffer.pop(val) && val == 30);
    assert(buffer.pop(val) && val == 40);
    assert(buffer.pop(val) && val == 50);

    assert(buffer.empty());
    assert(!buffer.pop(val));
}

void testIrProtocolPulseParsingAndFormatting() {
    std::cout << "  [CORE] Testing pulse parsing & formatting..." << std::endl;

    std::string input = "[9000, -4500, 560, -560, 560, -1690]";
    std::vector<int> pulses = ir::IrProtocol::parsePulseSequence(input);

    assert(pulses.size() == 6);
    assert(pulses[0] == 9000);
    assert(pulses[1] == -4500);
    assert(pulses[2] == 560);
    assert(pulses[3] == -560);

    std::string formatted = ir::IrProtocol::formatPulseSequence(pulses);
    assert(formatted == "9000, -4500, 560, -560, 560, -1690");

    auto sigOpt = ir::IrSignal::parseString(input, 38);
    assert(sigOpt.has_value());
    assert(sigOpt->pulses.size() == 6);
    assert(sigOpt->frequency_khz == 38);
}

void testIrProtocolAGCLeaderFix() {
    std::cout << "  [CORE] Testing AGC header mark correction..." << std::endl;

    std::vector<int> pulses = { 6944, -4576, 544, -608, 544, -1728 };
    ir::IrProtocol::fixLeadingMark(pulses);

    assert(pulses[0] == 9000); // Leader mark corrected from 6944 to 9000
    assert(pulses[1] == -4576);
}

void testTiqiaaPayloadEncodingAndDecoding() {
    std::cout << "  [CORE] Testing Tiqiaa hardware payload roundtrip..." << std::endl;

    std::vector<int> originalPulses = { 9000, -4500, 560, -560, 560, -1690 };
    uint8_t freq = 38;

    auto encodedBytes = ir::IrProtocol::encodeTiqiaaPayload(originalPulses, freq);
    assert(!encodedBytes.empty());
    assert(encodedBytes[0] == 38); // First byte is carrier frequency

    // Construct full dummy Tiqiaa packet framing: ST <id> D <encodedBytes> EN
    std::vector<uint8_t> frame;
    frame.push_back('S');
    frame.push_back('T');
    frame.push_back(1); // cmd_id
    frame.push_back('D'); // cmd_char

    frame.insert(frame.end(), encodedBytes.begin(), encodedBytes.end());

    frame.push_back('E');
    frame.push_back('N');

    ir::IrSignal decodedSignal;
    bool ok = ir::IrProtocol::decodeTiqiaaPayload(frame, decodedSignal);
    assert(ok);
    assert(decodedSignal.frequency_khz == 38);
    assert(decodedSignal.pulses.size() == originalPulses.size());

    for (size_t i = 0; i < originalPulses.size(); ++i) {
        assert(std::abs(decodedSignal.pulses[i] - originalPulses[i]) <= 16);
    }

    // Verify payload containing 0x45, 0x4E ('E', 'N') inside data does not trigger false early termination
    std::vector<uint8_t> frameWithInternalEn = {
        'S', 'T', 1, 'D', 38,
        0xFF, 0x45, 0x4E, 0x85, // 0x45 ('E') and 0x4E ('N') inside pulse data
        0x10, 0x90,
        'E', 'N'                // Real terminator at the end
    };
    ir::IrSignal decodedWithInternalEn;
    bool okInternal = ir::IrProtocol::decodeTiqiaaPayload(frameWithInternalEn, decodedWithInternalEn);
    assert(okInternal);
    assert(decodedWithInternalEn.pulses.size() >= 3);

    // 3. Test decoding user's Hisense TV capture raw hex payload with 0xFF frequency byte and 72-pulse repeat frame
    std::string rawHexHisense = "53540044ffffffffc17f7f1da423a522a522a423a423a622a423a423a469a667a469a667a423a469a469a469a469a567a469a423a522a423a622a423a622a622a622a468a567a469a667a468a47f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f7f18ffffffffc27f0ea613454e";
    std::vector<uint8_t> hisenseRawBytes;
    for (size_t i = 0; i < rawHexHisense.length(); i += 2) {
        hisenseRawBytes.push_back(static_cast<uint8_t>(std::stoul(rawHexHisense.substr(i, 2), nullptr, 16)));
    }
    ir::IrSignal decodedHisenseSig;
    bool hisenseRawOk = ir::IrProtocol::decodeTiqiaaPayload(hisenseRawBytes, decodedHisenseSig);
    assert(hisenseRawOk);
    assert(decodedHisenseSig.frequency_khz == 38); // Sanitized from 0xFF (255) to 38 kHz
    assert(decodedHisenseSig.pulses.size() == 68); // Normalized from 72 to 68 pulses

    ir::DecodedCode hisenseParsedCode;
    bool parsedHisenseOk = ir::IrProtocol::decodeNec(decodedHisenseSig.pulses, hisenseParsedCode);
    assert(parsedHisenseOk);
    assert(hisenseParsedCode.address == 0xEF00);
    assert(hisenseParsedCode.command == 0x07);

    // 4. Test encoding with uninitialized / out-of-band frequency (0xFF) sanitizes to 38 kHz
    auto encUnsanitizedFreq = ir::IrProtocol::encodeTiqiaaPayload(originalPulses, 0xFF);
    assert(!encUnsanitizedFreq.empty());
    assert(encUnsanitizedFreq[0] == 38);
}

void testNecProtocolCodec() {
    std::cout << "  [CORE] Testing NEC protocol encode & decode..." << std::endl;

    // 1. Standard 8-bit NEC
    uint32_t address = 0x00;
    uint32_t command = 0x45;

    std::vector<int> necPulses = ir::IrProtocol::encodeNec(address, command);
    assert(necPulses.size() == 68);

    ir::DecodedCode code;
    bool ok = ir::IrProtocol::decodeNec(necPulses, code);
    assert(ok);
    assert(code.protocol == ir::IrProtocolType::NEC);
    assert(code.address == address);
    assert(code.command == command);
    assert(!code.isRepeat);

    // 2. Extended 16-bit NEC with non-complementary address (e.g., 0x1234)
    uint32_t extAddr = 0x1234;
    uint32_t extCmd = 0x71;
    std::vector<int> extPulses = ir::IrProtocol::encodeNec(extAddr, extCmd);
    assert(extPulses.size() == 68);

    ir::DecodedCode extCode;
    bool extOk = ir::IrProtocol::decodeNec(extPulses, extCode);
    assert(extOk);
    assert(extCode.protocol == ir::IrProtocolType::NEC);
    assert(extCode.address == extAddr);
    assert(extCode.command == extCmd);

    // 3. Hisense TV complementary address (0x04FB where 0x04 == ~0xFB, decoded as 8-bit address 0xFB)
    uint32_t hisenseAddr = 0x04FB;
    uint32_t hisenseCmd = 0x71;
    std::vector<int> hisensePulses = ir::IrProtocol::encodeNec(hisenseAddr, hisenseCmd);
    assert(hisensePulses.size() == 68);

    ir::DecodedCode hisenseCode;
    bool hisenseOk = ir::IrProtocol::decodeNec(hisensePulses, hisenseCode);
    assert(hisenseOk);
    assert(hisenseCode.protocol == ir::IrProtocolType::NEC);
    assert(hisenseCode.address == 0xFB);
    assert(hisenseCode.command == hisenseCmd);

    // 4. Decoding signal with leading idle space and noise glitch
    std::vector<int> noisyPulses = { -320, 150, -200 };
    noisyPulses.insert(noisyPulses.end(), hisensePulses.begin(), hisensePulses.end());
    ir::DecodedCode noisyCode;
    bool noisyOk = ir::IrProtocol::decodeNec(noisyPulses, noisyCode);
    assert(noisyOk);
    assert(noisyCode.address == 0xFB);
    assert(noisyCode.command == hisenseCmd);

    // 5. Hisense TV remote captured 72-pulse signal (68 NEC pulses + repeat frame header)
    std::vector<int> hisenseCaptured72 = {
        9000, -4528, 576, -560, 592, -544, 592, -544, 576, -560, 576, -560, 608, -544, 576, -560, 576, -560,
        576, -1680, 608, -1648, 576, -1680, 608, -1648, 576, -560, 576, -1680, 576, -1680, 576, -1680,
        576, -1680, 592, -1648, 576, -1680, 576, -560, 592, -544, 576, -560, 608, -544, 576, -560,
        608, -544, 608, -544, 608, -544, 576, -1664, 592, -1648, 576, -1680, 608, -1648, 576, -1664,
        576, -41024, 9184, -2256, 608, -304
    };
    assert(hisenseCaptured72.size() == 72);
    bool normOk = ir::IrProtocol::normalizeNec(hisenseCaptured72);
    assert(normOk);
    assert(hisenseCaptured72.size() == 68);
    assert(hisenseCaptured72[0] == 9000);
    assert(hisenseCaptured72[1] == -4500);
    assert(hisenseCaptured72[66] == 560);
    assert(hisenseCaptured72[67] == -40000);

    ir::DecodedCode hisenseNormDecoded;
    bool hisenseNormDecOk = ir::IrProtocol::decodeNec(hisenseCaptured72, hisenseNormDecoded);
    assert(hisenseNormDecOk);
    assert(hisenseNormDecoded.protocol == ir::IrProtocolType::NEC);
    assert(hisenseNormDecoded.address == 0xEF00);
    assert(hisenseNormDecoded.command == 0x07);
}

void testMultiChunkPayloadAssembly() {
    std::cout << "  [CORE] Testing multi-chunk TV remote payload reassembly..." << std::endl;

    // Simulate standard TV remote button press (NEC protocol: 68 pulses)
    uint32_t tvAddress = 0x00;
    uint32_t tvCommand = 0x18; // Power button
    std::vector<int> tvPulses = ir::IrProtocol::encodeNec(tvAddress, tvCommand);
    assert(tvPulses.size() == 68);

    uint8_t freq = 38;
    auto encodedBytes = ir::IrProtocol::encodeTiqiaaPayload(tvPulses, freq);

    // Frame: ST <id> D <encodedBytes> EN
    std::vector<uint8_t> fullFrame;
    fullFrame.push_back('S');
    fullFrame.push_back('T');
    fullFrame.push_back(1); // cmd_id
    fullFrame.push_back('D');
    fullFrame.insert(fullFrame.end(), encodedBytes.begin(), encodedBytes.end());
    fullFrame.push_back('E');
    fullFrame.push_back('N');

    // TV remote frame is ~103 bytes, which exceeds 56 bytes and MUST be fragmented into 2 chunks
    constexpr size_t maxChunk = 56;
    size_t totalChunks = (fullFrame.size() + maxChunk - 1) / maxChunk;
    assert(totalChunks == 2);

    // Build USB packet 1 (chunk 1/2)
    std::vector<uint8_t> chunk1(fullFrame.begin(), fullFrame.begin() + maxChunk);
    std::vector<uint8_t> usbPkt1(5 + chunk1.size());
    usbPkt1[0] = 0x02; // Report ID
    usbPkt1[1] = static_cast<uint8_t>(chunk1.size() + 3);
    usbPkt1[2] = 1;    // counter
    usbPkt1[3] = static_cast<uint8_t>(totalChunks); // 2
    usbPkt1[4] = 1;    // chunk index 1
    std::copy(chunk1.begin(), chunk1.end(), usbPkt1.begin() + 5);

    // Build USB packet 2 (chunk 2/2)
    std::vector<uint8_t> chunk2(fullFrame.begin() + maxChunk, fullFrame.end());
    std::vector<uint8_t> usbPkt2(5 + chunk2.size());
    usbPkt2[0] = 0x02;
    usbPkt2[1] = static_cast<uint8_t>(chunk2.size() + 3);
    usbPkt2[2] = 1;
    usbPkt2[3] = static_cast<uint8_t>(totalChunks); // 2
    usbPkt2[4] = 2;    // chunk index 2
    std::copy(chunk2.begin(), chunk2.end(), usbPkt2.begin() + 5);

    // 1. Chunk 1 alone must FAIL to decode because 'EN' is missing (prevents premature return)
    ir::IrSignal partialSignal;
    bool partialOk = ir::IrProtocol::decodeTiqiaaPayload(chunk1, partialSignal);
    assert(!partialOk);

    // 2. Reassemble fragments
    std::vector<uint8_t> reassembled;
    reassembled.insert(reassembled.end(), usbPkt1.begin() + 5, usbPkt1.end());
    reassembled.insert(reassembled.end(), usbPkt2.begin() + 5, usbPkt2.end());
    assert(reassembled.size() == fullFrame.size());

    // 3. Fully reassembled payload decodes successfully
    ir::IrSignal decodedTvSignal;
    bool reassembledOk = ir::IrProtocol::decodeTiqiaaPayload(reassembled, decodedTvSignal);
    assert(reassembledOk);
    assert(decodedTvSignal.frequency_khz == 38);

    // 4. Verify decoded signal decodes to exact NEC TV Remote command
    ir::DecodedCode decodedNec;
    bool necOk = ir::IrProtocol::decodeNec(decodedTvSignal.pulses, decodedNec);
    assert(necOk);
    assert(decodedNec.protocol == ir::IrProtocolType::NEC);
    assert(decodedNec.address == tvAddress);
    assert(decodedNec.command == tvCommand);
}

void testIrUsbTransceiverClass() {
    std::cout << "  [CORE] Testing IrUsbTransceiver lifecycle..." << std::endl;

    ir::IrUsbTransceiver transceiver(38);
    assert(!transceiver.isConnected());
    assert(transceiver.getState() == ir::DeviceState::Disconnected);

    bool logCalled = false;
    transceiver.setLogCallback([&logCalled](const std::string&) {
        logCalled = true;
    });

    bool opened = transceiver.openDevice();
    std::cout << "         Hardware status: " << (opened ? "Connected" : "Not connected (standalone test)") << std::endl;

    transceiver.closeDevice();
    assert(!transceiver.isConnected());
}

void testSignalStorageAndMatching() {
    std::cout << "  [CORE] Testing SignalStorage and pulse matching..." << std::endl;

    std::string testPath = "/tmp/test_unified_storage.json";
    std::remove(testPath.c_str());

    {
        ir::SignalStorage storage(testPath);
        assert(storage.count() == 0);

        ir::SavedIrSignal sig1;
        sig1.name = "Test TV Power";
        sig1.protocol = "NEC";
        sig1.rawHex = "0xE718FF00";
        sig1.frequency_khz = 38;
        sig1.pulses = ir::IrProtocol::encodeNec(0x00, 0x18);
        sig1.timestamp = "2026-10-03 21:00:00";
        sig1.shellCommand = "echo TV Power";

        storage.addSignal(sig1);
        assert(storage.count() == 1);
        assert(storage.getSignals()[0].name == "Test TV Power");
        assert(storage.getSignals()[0].shellCommand == "echo TV Power");

        ir::SavedIrSignal sig1Updated;
        sig1Updated.name = "Test TV Power Updated";
        sig1Updated.protocol = "Raw";
        sig1Updated.rawHex = "0xAA55";
        sig1Updated.frequency_khz = 40;
        sig1Updated.pulses = { 1000, -1000, 2000, -2000 };
        sig1Updated.timestamp = "2026-10-04 11:00:00";
        sig1Updated.shellCommand = "amixer set Master 5%+";
        assert(storage.updateSignal(0, sig1Updated));
        assert(storage.getSignals()[0].name == "Test TV Power Updated");
        assert(storage.getSignals()[0].frequency_khz == 40);
        assert(storage.getSignals()[0].shellCommand == "amixer set Master 5%+");
    }

    {
        ir::SignalStorage storage2(testPath);
        assert(storage2.count() == 1);
        assert(storage2.getSignals()[0].name == "Test TV Power Updated");
        assert(storage2.getSignals()[0].frequency_khz == 40);
        assert(storage2.getSignals()[0].shellCommand == "amixer set Master 5%+");

        // 1. Exact pulse match
        auto updatedPulses = storage2.getSignals()[0].pulses;
        auto matchExact = storage2.findMatchingSignal(updatedPulses);
        assert(matchExact.has_value());
        assert(matchExact->name == "Test TV Power Updated");

        // 2. Decoded signal match
        ir::IrSignal incoming;
        incoming.pulses = updatedPulses;
        incoming.frequency_khz = 40;
        auto matchSig = storage2.findMatchingSignal(incoming);
        assert(matchSig.has_value());
        assert(matchSig->name == "Test TV Power Updated");

        // 3. Timing-tolerant pulse sequence match
        std::vector<int> jitteredPulses = updatedPulses;
        for (size_t i = 2; i < jitteredPulses.size() - 1; ++i) {
            if (jitteredPulses[i] > 0) jitteredPulses[i] += 40;
            else jitteredPulses[i] -= 40;
        }
        auto matchJitter = storage2.findMatchingSignal(jitteredPulses);
        assert(matchJitter.has_value());
        assert(matchJitter->name == "Test TV Power Updated");

        // 4. Mismatched signal
        auto otherPulses = ir::IrProtocol::encodeNec(0x05, 0x99);
        auto matchOther = storage2.findMatchingSignal(otherPulses);
        assert(!matchOther.has_value());
    }

    std::remove(testPath.c_str());
}

void testSignalStorageUsesApplicationDataFolder() {
    std::cout << "  [CORE] Testing SignalStorage app-data path selection..." << std::endl;

    const std::string defaultPath = ir::SignalStorage::getDefaultStoragePath();
    assert(!defaultPath.empty());
    assert(defaultPath.find("saved_ir_signals.json") != std::string::npos);
#if defined(_WIN32)
    assert(defaultPath.find("IR") != std::string::npos);
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        assert(defaultPath.find("IR") != std::string::npos);
    } else {
        assert(defaultPath.find(".config") != std::string::npos || defaultPath.find(".local") != std::string::npos);
    }
#endif
}

void testRemoteControlDeviceStorageAndMigration() {
    std::cout << "  [CORE] Testing Remote Control Device (RCD) storage and migration..." << std::endl;

    std::string testPath = "/tmp/test_rcd_storage.json";
    std::remove(testPath.c_str());

    // 1. Write an OLD format JSON file (flat array of signals)
    {
        std::ofstream oldFile(testPath);
        oldFile << R"([
          {
            "name": "Legacy TV Power",
            "protocol": "NEC",
            "frequency": 38,
            "timestamp": "2026-10-01 10:00:00",
            "rawHex": "0x12345678",
            "pulses": [9000, -4500, 560, -560]
          },
          {
            "name": "Legacy TV Mute",
            "protocol": "NEC",
            "frequency": 38,
            "timestamp": "2026-10-01 10:05:00",
            "rawHex": "0x87654321",
            "pulses": [9000, -4500, 560, -1690]
          }
        ])";
        oldFile.close();
    }

    // 2. Load with SignalStorage - should migrate to "Default Remote Device"
    {
        ir::SignalStorage storage(testPath);
        assert(storage.count() == 2);
        assert(storage.deviceCount() == 1);
        assert(storage.hasDevice("Default Remote Device"));
        assert(storage.count("Default Remote Device") == 2);

        auto dev = storage.getDevice("Default Remote Device");
        assert(dev != nullptr);
        assert(dev->savedSignals.size() == 2);
        assert(dev->savedSignals[0].name == "Legacy TV Power");
        assert(dev->savedSignals[0].rcdName == "Default Remote Device");
        assert(dev->savedSignals[1].name == "Legacy TV Mute");
        assert(dev->savedSignals[1].rcdName == "Default Remote Device");

        // 3. Add a signal to a NEW RCD: "Living Room TV"
        ir::SavedIrSignal newSig;
        newSig.name = "Volume Up";
        newSig.protocol = "NEC";
        newSig.frequency_khz = 38;
        newSig.pulses = ir::IrProtocol::encodeNec(0x12, 0x34);
        newSig.rawHex = "0x55AA55AA";
        newSig.timestamp = "2026-10-17 12:00:00";

        storage.addSignal(newSig, "Living Room TV");
        assert(storage.deviceCount() == 2);
        assert(storage.count() == 3);
        assert(storage.count("Living Room TV") == 1);
        assert(storage.count("Default Remote Device") == 2);

        // 4. Add a signal to an EXISTING RCD: "Living Room TV"
        ir::SavedIrSignal newSig2 = newSig;
        newSig2.name = "Volume Down";
        storage.addSignal(newSig2, "Living Room TV");
        assert(storage.deviceCount() == 2);
        assert(storage.count() == 4);
        assert(storage.count("Living Room TV") == 2);

        // 5. Test findMatchingSignalInDevice
        auto matchInDev = storage.findMatchingSignalInDevice("Living Room TV", newSig.pulses);
        assert(matchInDev.has_value());
        assert(matchInDev->name == "Volume Up");
        assert(matchInDev->rcdName == "Living Room TV");

        auto matchNotInDev = storage.findMatchingSignalInDevice("Default Remote Device", newSig.pulses);
        assert(!matchNotInDev.has_value());

        // Global findMatchingSignal returns rcdName
        auto matchGlobal = storage.findMatchingSignal(newSig.pulses);
        assert(matchGlobal.has_value());
        assert(matchGlobal->rcdName == "Living Room TV");

        // 6. Test "each RCD contains 1 .. n signals":
        // Removing all signals from "Living Room TV" must remove the device
        assert(storage.removeSignal("Living Room TV", 0)); // removed "Volume Up", 1 left
        assert(storage.deviceCount() == 2);
        assert(storage.count("Living Room TV") == 1);
        assert(storage.removeSignal("Living Room TV", 0)); // removed "Volume Down", 0 left
        // Device should now be automatically pruned!
        assert(storage.deviceCount() == 1);
        assert(!storage.hasDevice("Living Room TV"));
        assert(storage.count() == 2);
    }

    // 7. Verify persistent file on disk is in the new RCD format
    {
        ir::SignalStorage storage(testPath);
        assert(storage.deviceCount() == 1);
        assert(storage.hasDevice("Default Remote Device"));
        assert(storage.count() == 2);
    }

    std::remove(testPath.c_str());
}


// ============================================================================
// PART 2: REST HTTP API SERVER INTEGRATION TESTS
// ============================================================================

void testApiServerEndpoints() {
    std::cout << "  [API] Testing REST HTTP API endpoints..." << std::endl;

    const std::string testStoragePath = "/tmp/test_api_unified.json";
    std::remove(testStoragePath.c_str());

    auto transceiver = std::make_shared<ir::IrUsbTransceiver>(38);
    auto storage = std::make_shared<ir::SignalStorage>(testStoragePath);

    // Initial signal in storage
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
    config.port = 19190;
    config.enableCors = true;
    config.autoStartPolling = false;

    ir::HttpServerModule server(transceiver, storage, config);
    bool started = server.start();
    assert(started);
    assert(server.isRunning());

    httplib::Client client("127.0.0.1", 19190);
    client.set_connection_timeout(2);
    client.set_read_timeout(2);

    // 1. GET /
    {
        auto res = client.Get("/");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("service"));
        assert(j.contains("version"));
        assert(j.contains("endpoints"));
        assert(res->has_header("Access-Control-Allow-Origin"));
    }

    // 2. GET /api/v1/status
    {
        auto res = client.Get("/api/v1/status");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("connected"));
        assert(j.contains("state"));
        assert(j.contains("device"));
        assert(j["device"].contains("vendorId"));
        assert(j["device"].contains("productId"));
    }

    // 3. GET /api/v1/receive when buffer is empty
    {
        auto res = client.Get("/api/v1/receive");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("available"));
        assert(j["available"] == false);
    }

    // 4. GET / should no longer advertise storage management endpoints
    {
        auto res = client.Get("/");
        assert(res && res->status == 200);
        auto j = nlohmann::json::parse(res->body);
        assert(j.contains("endpoints"));
        auto endpoints = j["endpoints"];
        assert(!endpoints.contains("GET /api/v1/signals"));
        assert(!endpoints.contains("POST /api/v1/signals"));
    }

    // 5. POST /api/v1/send input validation
    {
        auto res1 = client.Post("/api/v1/send", "invalid_json_text", "application/json");
        assert(res1 && (res1->status == 400 || res1->status == 503));

        nlohmann::json validPayload = {
            {"protocol", "NEC"},
            {"code", "0x00FF45BA"}
        };
        auto res2 = client.Post("/api/v1/send", validPayload.dump(), "application/json");
        assert(res2);
        if (!transceiver->isConnected()) {
            assert(res2->status == 503);
        }
    }

    // Stop server
    server.stop();
    assert(!server.isRunning());

    std::remove(testStoragePath.c_str());
}

// ============================================================================
// MAIN UNIFIED TEST RUNNER
// ============================================================================

int main(int argc, char* argv[]) {
    std::string filter;
    if (argc > 1) {
        filter = argv[1];
    }

    if (!filter.empty() && filter != "all") {
        if (filter == "ring_buffer") {
            testRingBuffer();
        } else if (filter == "pulse_parsing") {
            testIrProtocolPulseParsingAndFormatting();
        } else if (filter == "agc_leader") {
            testIrProtocolAGCLeaderFix();
        } else if (filter == "tiqiaa_payload") {
            testTiqiaaPayloadEncodingAndDecoding();
        } else if (filter == "multi_chunk") {
            testMultiChunkPayloadAssembly();
        } else if (filter == "nec_codec") {
            testNecProtocolCodec();
        } else if (filter == "transceiver") {
            testIrUsbTransceiverClass();
        } else if (filter == "storage_matching") {
            testSignalStorageAndMatching();
        } else if (filter == "rest_api") {
            testApiServerEndpoints();
        } else {
            std::cerr << "Unknown test filter: " << filter << "\n";
            std::cerr << "Supported tests: ring_buffer, pulse_parsing, agc_leader, tiqiaa_payload, multi_chunk, nec_codec, transceiver, storage_matching, rest_api\n";
            return 1;
        }
        std::cout << "[PASSED] Test " << filter << " completed successfully.\n";
        return 0;
    }

    std::cout << "========================================================\n";
    std::cout << " RUNNING UNIFIED IR TEST SUITE (CORE + REST API)\n";
    std::cout << "========================================================\n";

    std::cout << "\n[STAGE 1] Testing ir_core Engine Components:\n";
    testRingBuffer();
    testIrProtocolPulseParsingAndFormatting();
    testIrProtocolAGCLeaderFix();
    testTiqiaaPayloadEncodingAndDecoding();
    testMultiChunkPayloadAssembly();
    testNecProtocolCodec();
    testIrUsbTransceiverClass();
    testSignalStorageAndMatching();
    testSignalStorageUsesApplicationDataFolder();
    testRemoteControlDeviceStorageAndMigration();
    std::cout << "-> All ir_core tests passed!\n";

    std::cout << "\n[STAGE 2] Testing ir_api REST HTTP Service:\n";
    testApiServerEndpoints();
    std::cout << "-> All ir_api tests passed!\n";

    std::cout << "\n========================================================\n";
    std::cout << " SUCCESS: ALL 8 TEST SUITES PASSED CLEANLY!\n";
    std::cout << "========================================================\n";
    return 0;
}
