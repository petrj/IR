#include "IrProtocol.h"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iomanip>

namespace ir {

std::string IrSignal::toString() const {
    return IrProtocol::formatPulseSequence(pulses);
}

std::optional<IrSignal> IrSignal::parseString(const std::string& input, uint8_t defaultFreq) {
    auto parsedPulses = IrProtocol::parsePulseSequence(input);
    if (parsedPulses.empty()) {
        return std::nullopt;
    }
    IrSignal sig;
    sig.pulses = std::move(parsedPulses);
    sig.frequency_khz = defaultFreq;
    sig.timestamp = std::chrono::system_clock::now();
    return sig;
}

std::string DecodedCode::protocolName() const {
    switch (protocol) {
        case IrProtocolType::NEC: return "NEC";
        case IrProtocolType::Sony: return "Sony";
        case IrProtocolType::RC5: return "RC5";
        case IrProtocolType::Raw: return "Raw";
        default: return "Unknown";
    }
}

std::string DecodedCode::toString() const {
    std::ostringstream ss;
    ss << "Protocol: " << protocolName();
    if (protocol != IrProtocolType::Raw && protocol != IrProtocolType::Unknown) {
        ss << " | Addr: 0x" << std::hex << std::uppercase << (address > 0xFF ? std::setw(4) : std::setw(2)) << std::setfill('0') << address
           << " | Cmd: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << command
           << " | Raw: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << rawValue;
        if (isRepeat) ss << " [REPEAT]";
    }
    return ss.str();
}

void IrProtocol::fixLeadingMark(std::vector<int>& pulses, bool verbose) {
    constexpr int NEC_LEADER_MARK_US = 9000;
    // Skip any leading spaces if present
    size_t start = 0;
    while (start < pulses.size() && pulses[start] <= 0) {
        start++;
    }

    if (start + 1 < pulses.size() && pulses[start] > 0 && pulses[start + 1] < 0) {
        int space = std::abs(pulses[start + 1]);
        if (space >= 3000 && space <= 6000 && pulses[start] >= 1500 && pulses[start] < 8000) {
            if (verbose) {
                std::cout << "[IrProtocol] Correcting short AGC leading mark "
                          << pulses[start] << " us -> " << NEC_LEADER_MARK_US << " us\n";
            }
            pulses[start] = NEC_LEADER_MARK_US;
        }
    }
}

std::vector<uint8_t> IrProtocol::encodeTiqiaaPayload(const std::vector<int>& pulses, uint8_t frequency_khz) {
    std::vector<int> fixedPulses = pulses;
    fixLeadingMark(fixedPulses);

    std::vector<uint8_t> extraBytes;
    extraBytes.push_back(frequency_khz);

    for (int p : fixedPulses) {
        uint8_t stateBit = (p > 0) ? 0x80 : 0x00;
        int durationUnits = static_cast<int>(std::round(std::abs(p) / 16.0));

        if (durationUnits == 0) {
            continue;
        }

        while (durationUnits > 127) {
            extraBytes.push_back(stateBit | 127);
            durationUnits -= 127;
        }
        if (durationUnits > 0) {
            extraBytes.push_back(stateBit | static_cast<uint8_t>(durationUnits));
        }
    }

    return extraBytes;
}

bool IrProtocol::decodeTiqiaaPayload(const std::vector<uint8_t>& payload, IrSignal& outSignal, bool verbose) {
    if (payload.empty()) {
        return false;
    }

    // Locate 'D' marker
    auto dIt = std::find(payload.begin(), payload.end(), static_cast<uint8_t>('D'));
    if (dIt == payload.end()) {
        if (verbose) {
            std::cout << "[IrProtocol] No 'D' marker found in payload\n";
        }
        return false;
    }

    size_t dPos = std::distance(payload.begin(), dIt);
    if (dPos + 1 >= payload.size()) {
        return false;
    }

    uint8_t frequency_khz = payload[dPos + 1];
    size_t irDataStart = dPos + 2;

    // Locate 'EN' terminator (0x45, 0x4E) searching backwards from the end of payload
    // to avoid matching false positives (0x45 = 1104us, 0x4E = 1248us) within pulse data.
    size_t enPos = std::string::npos;
    if (payload.size() >= irDataStart + 2) {
        for (size_t i = payload.size(); i >= irDataStart + 2; --i) {
            if (payload[i - 2] == 'E' && payload[i - 1] == 'N') {
                enPos = i - 2;
                break;
            }
        }
    }

    if (enPos == std::string::npos) {
        if (verbose) {
            std::cout << "[IrProtocol] Missing 'EN' terminator, frame incomplete\n";
        }
        return false;
    }

    std::vector<int> mergedPulses;
    for (size_t i = irDataStart; i < enPos; ++i) {
        uint8_t b = payload[i];
        bool stateOn = (b & 0x80) != 0;
        int duration = (b & 0x7F) * 16;

        if (duration == 0) {
            continue;
        }

        int value = stateOn ? duration : -duration;

        if (!mergedPulses.empty() && ((mergedPulses.back() > 0) == stateOn)) {
            mergedPulses.back() += value;
        } else {
            mergedPulses.push_back(value);
        }
    }

    // Filter out noise pulses < 50 us
    std::vector<int> pulses;
    for (int p : mergedPulses) {
        if (std::abs(p) >= 50) {
            pulses.push_back(p);
        }
    }

    // Drop leading idle spaces / preamble gaps before the first active burst
    while (!pulses.empty() && pulses.front() <= 0) {
        pulses.erase(pulses.begin());
    }

    // If there is leading short noise glitch (< 400us mark + space) before the real leader (>= 6000us)
    if (pulses.size() >= 3 && pulses[0] > 0 && pulses[0] < 400 && pulses[1] < 0 && pulses[2] >= 6000) {
        pulses.erase(pulses.begin(), pulses.begin() + 2);
    }

    if (pulses.size() < 4) {
        if (verbose) {
            std::cout << "[IrProtocol] Too few valid pulses (< 4)\n";
        }
        return false;
    }

    fixLeadingMark(pulses, verbose);

    outSignal.pulses = std::move(pulses);
    outSignal.frequency_khz = frequency_khz;
    outSignal.timestamp = std::chrono::system_clock::now();

    return true;
}

std::vector<int> IrProtocol::parsePulseSequence(const std::string& inputStr) {
    std::string cleaned = inputStr;

    // Strip outer brackets if present
    size_t start = cleaned.find_first_not_of(" \t\r\n[");
    size_t end = cleaned.find_last_not_of(" \t\r\n]");
    if (start == std::string::npos) {
        return {};
    }
    cleaned = cleaned.substr(start, end - start + 1);

    // Replace commas with spaces
    std::replace(cleaned.begin(), cleaned.end(), ',', ' ');

    std::istringstream iss(cleaned);
    std::vector<int> pulses;
    int val;
    while (iss >> val) {
        pulses.push_back(val);
    }
    return pulses;
}

std::string IrProtocol::formatPulseSequence(const std::vector<int>& pulses) {
    std::ostringstream ss;
    for (size_t i = 0; i < pulses.size(); ++i) {
        ss << pulses[i];
        if (i + 1 < pulses.size()) {
            ss << ", ";
        }
    }
    return ss.str();
}

bool IrProtocol::decodeNec(const std::vector<int>& pulses, DecodedCode& outCode) {
    if (pulses.size() < 4) {
        return false;
    }

    // Find first active mark (skipping any leading spaces)
    size_t startIdx = 0;
    while (startIdx < pulses.size() && pulses[startIdx] <= 0) {
        startIdx++;
    }
    if (startIdx + 3 >= pulses.size()) {
        return false;
    }

    // Skip short noise glitch before leader mark if present
    if (startIdx + 4 < pulses.size() && pulses[startIdx] > 0 && pulses[startIdx] < 400 &&
        pulses[startIdx + 1] < 0 && pulses[startIdx + 2] >= 6000) {
        startIdx += 2;
    }

    int leaderMark = pulses[startIdx];
    int leaderSpace = std::abs(pulses[startIdx + 1]);

    // Check NEC Leader Mark (~9000 us nominal, allow 6500 to 11500 us)
    if (leaderMark < 6500 || leaderMark > 11500) {
        return false;
    }

    // Check NEC Repeat frame (~2250 us space, allow 1500 to 3000 us)
    if (leaderSpace >= 1500 && leaderSpace <= 3000) {
        outCode.protocol = IrProtocolType::NEC;
        outCode.isRepeat = true;
        return true;
    }

    // Standard NEC leader space (~4500 us nominal, allow 3000 to 6000 us)
    if (leaderSpace < 3000 || leaderSpace > 6000) {
        return false;
    }

    uint32_t rawValue = 0;
    size_t bitCount = 0;

    for (size_t i = startIdx + 2; i + 1 < pulses.size() && bitCount < 32; i += 2) {
        int mark = pulses[i];
        int space = std::abs(pulses[i + 1]);

        // Nominal mark is 560 us; allow 180 to 1100 us
        if (mark < 180 || mark > 1100) {
            break;
        }

        // Decision threshold between bit 0 (~560 us) and bit 1 (~1690 us) is ~1050 us
        if (space >= 180 && space < 1050) {
            // Bit 0
            rawValue = (rawValue >> 1); // LSB first
            bitCount++;
        } else if (space >= 1050 && space <= 2500) {
            // Bit 1
            rawValue = (rawValue >> 1) | 0x80000000;
            bitCount++;
        } else {
            break;
        }
    }

    if (bitCount != 32) {
        return false;
    }

    outCode.protocol = IrProtocolType::NEC;
    outCode.rawValue = rawValue;

    uint8_t addrLow = rawValue & 0xFF;
    uint8_t addrHigh = (rawValue >> 8) & 0xFF;
    if (addrHigh == static_cast<uint8_t>(~addrLow)) {
        // Standard 8-bit NEC (second byte is bitwise complement)
        outCode.address = addrLow;
    } else {
        // Extended 16-bit NEC (e.g. Hisense TV 0x04FB, 0xF708, etc.)
        outCode.address = (static_cast<uint32_t>(addrHigh) << 8) | addrLow;
    }

    outCode.command = (rawValue >> 16) & 0xFF;
    outCode.bitCount = 32;
    outCode.isRepeat = false;

    return true;
}

std::vector<int> IrProtocol::encodeNec(uint32_t address, uint32_t command) {
    std::vector<int> pulses;
    pulses.reserve(68);

    // NEC Leader
    pulses.push_back(9000);
    pulses.push_back(-4500);

    // 32-bit payload:
    // If address > 0xFF, it's a 16-bit extended address (e.g. Hisense TV 0x04FB)
    // where byte 0 = address & 0xFF, byte 1 = (address >> 8) & 0xFF.
    // Otherwise standard 8-bit address where byte 0 = address, byte 1 = ~address.
    uint32_t addrByte0 = address & 0xFF;
    uint32_t addrByte1 = (address > 0xFF) ? ((address >> 8) & 0xFF) : ((~address) & 0xFF);
    uint32_t cmdByte0 = command & 0xFF;
    uint32_t cmdByte1 = (~command) & 0xFF;
    uint32_t rawValue = addrByte0 | (addrByte1 << 8) | (cmdByte0 << 16) | (cmdByte1 << 24);

    for (int i = 0; i < 32; ++i) {
        pulses.push_back(560);
        if ((rawValue >> i) & 1) {
            pulses.push_back(-1690); // Bit 1
        } else {
            pulses.push_back(-560);  // Bit 0
        }
    }

    // Stop bit
    pulses.push_back(560);
    pulses.push_back(-40000); // trailing space

    return pulses;
}

} // namespace ir
