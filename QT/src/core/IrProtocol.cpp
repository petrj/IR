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
        ss << " | Addr: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << address
           << " | Cmd: 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << command
           << " | Raw: 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << rawValue;
        if (isRepeat) ss << " [REPEAT]";
    }
    return ss.str();
}

void IrProtocol::fixLeadingMark(std::vector<int>& pulses, bool verbose) {
    constexpr int NEC_LEADER_MARK_US = 9000;
    if (pulses.size() >= 2 && pulses[0] > 0 && pulses[1] < 0) {
        int space = std::abs(pulses[1]);
        if (space >= 3500 && space <= 5500 && pulses[0] < 8000) {
            if (verbose) {
                std::cout << "[IrProtocol] Correcting short AGC leading mark "
                          << pulses[0] << " us -> " << NEC_LEADER_MARK_US << " us\n";
            }
            pulses[0] = NEC_LEADER_MARK_US;
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

    // Locate 'EN' terminator (0x45, 0x4E)
    size_t enPos = std::string::npos;
    for (size_t i = irDataStart; i + 1 < payload.size(); ++i) {
        if (payload[i] == 'E' && payload[i + 1] == 'N') {
            enPos = i;
            break;
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

    int leaderMark = pulses[0];
    int leaderSpace = std::abs(pulses[1]);

    // Check NEC Leader Mark (~9000 us)
    if (leaderMark < 7500 || leaderMark > 10500) {
        return false;
    }

    // Check NEC Repeat frame (~2250 us space)
    if (leaderSpace >= 1750 && leaderSpace <= 2750) {
        outCode.protocol = IrProtocolType::NEC;
        outCode.isRepeat = true;
        return true;
    }

    // Standard NEC leader space (~4500 us)
    if (leaderSpace < 3500 || leaderSpace > 5500) {
        return false;
    }

    uint32_t rawValue = 0;
    size_t bitCount = 0;

    for (size_t i = 2; i + 1 < pulses.size() && bitCount < 32; i += 2) {
        int mark = pulses[i];
        int space = std::abs(pulses[i + 1]);

        if (mark < 300 || mark > 900) {
            break;
        }

        if (space >= 300 && space <= 900) {
            // Bit 0 (space ~560 us)
            rawValue = (rawValue >> 1); // LSB first
            bitCount++;
        } else if (space >= 1200 && space <= 2200) {
            // Bit 1 (space ~1690 us)
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
    outCode.address = rawValue & 0xFF;
    outCode.command = (rawValue >> 16) & 0xFF;
    outCode.bitCount = 32;
    outCode.isRepeat = false;

    return true;
}

std::vector<int> IrProtocol::encodeNec(uint32_t address, uint32_t command) {
    std::vector<int> pulses;
    pulses.reserve(67);

    // NEC Leader
    pulses.push_back(9000);
    pulses.push_back(-4500);

    // 32-bit payload: Address, ~Address, Command, ~Command
    uint32_t addrInv = (~address) & 0xFF;
    uint32_t cmdInv = (~command) & 0xFF;
    uint32_t rawValue = (address & 0xFF) | (addrInv << 8) | ((command & 0xFF) << 16) | (cmdInv << 24);

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
