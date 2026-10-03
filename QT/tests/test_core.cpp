#include "RingBuffer.h"
#include "IrProtocol.h"
#include "IrUsbTransceiver.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cmath>

void testRingBuffer() {
    std::cout << "[TEST] RingBuffer..." << std::endl;
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

    std::cout << "  PASSED: RingBuffer" << std::endl;
}

void testIrProtocolPulseParsingAndFormatting() {
    std::cout << "[TEST] IrProtocol pulse parsing & formatting..." << std::endl;

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

    std::cout << "  PASSED: Pulse parsing & formatting" << std::endl;
}

void testIrProtocolAGCLeaderFix() {
    std::cout << "[TEST] IrProtocol AGC leader fix..." << std::endl;

    std::vector<int> pulses = { 6944, -4576, 544, -608, 544, -1728 };
    ir::IrProtocol::fixLeadingMark(pulses);

    assert(pulses[0] == 9000); // Leader mark corrected from 6944 to 9000
    assert(pulses[1] == -4576);

    std::cout << "  PASSED: AGC leader fix" << std::endl;
}

void testTiqiaaPayloadEncodingAndDecoding() {
    std::cout << "[TEST] Tiqiaa payload encoding and decoding..." << std::endl;

    std::vector<int> originalPulses = { 9000, -4500, 560, -560, 560, -1690 };
    uint8_t freq = 38;

    auto encodedBytes = ir::IrProtocol::encodeTiqiaaPayload(originalPulses, freq);
    assert(!encodedBytes.empty());
    assert(encodedBytes[0] == 38); // First byte is frequency

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
        // Quantization error due to 16 us resolution should be <= 16 us
        assert(std::abs(decodedSignal.pulses[i] - originalPulses[i]) <= 16);
    }

    std::cout << "  PASSED: Tiqiaa payload encoding & decoding" << std::endl;
}

void testNecProtocolCodec() {
    std::cout << "[TEST] NEC protocol encode & decode..." << std::endl;

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

    std::cout << "  PASSED: NEC protocol encode & decode (" << code.toString() << ")" << std::endl;
}

void testIrUsbTransceiverClass() {
    std::cout << "[TEST] IrUsbTransceiver class lifecycle..." << std::endl;

    ir::IrUsbTransceiver transceiver(38);
    assert(!transceiver.isConnected());
    assert(transceiver.getState() == ir::DeviceState::Disconnected);

    // Test callback registration
    bool logCalled = false;
    transceiver.setLogCallback([&logCalled](const std::string& msg) {
        logCalled = true;
    });

    // Attempting open without device attached should cleanly return false or true depending on hardware
    bool opened = transceiver.openDevice();
    std::cout << "  Device open result: " << (opened ? "Hardware connected" : "Hardware not attached (expected)") << std::endl;

    transceiver.closeDevice();
    assert(!transceiver.isConnected());

    std::cout << "  PASSED: IrUsbTransceiver lifecycle" << std::endl;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << " Running IR Core Engine Unit Tests" << std::endl;
    std::cout << "==========================================" << std::endl;

    testRingBuffer();
    testIrProtocolPulseParsingAndFormatting();
    testIrProtocolAGCLeaderFix();
    testTiqiaaPayloadEncodingAndDecoding();
    testNecProtocolCodec();
    testIrUsbTransceiverClass();

    std::cout << "==========================================" << std::endl;
    std::cout << " ALL IR CORE TESTS PASSED SUCCESSFULLY! " << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
