#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

namespace ir {

/**
 * @brief Represents an IR pulse sequence.
 * Positive values indicate MARK (carrier ON, pulse duration in microseconds).
 * Negative values indicate SPACE (carrier OFF, duration in microseconds).
 */
struct IrSignal {
    std::vector<int> pulses;
    uint8_t frequency_khz{38};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    bool isValid() const {
        return !pulses.empty();
    }

    std::string toString() const;
    static std::optional<IrSignal> parseString(const std::string& input, uint8_t defaultFreq = 38);
};

enum class IrProtocolType {
    Raw,
    NEC,
    Sony,
    RC5,
    Unknown
};

/**
 * @brief Structure holding decoded IR protocol code details.
 */
struct DecodedCode {
    IrProtocolType protocol{IrProtocolType::Unknown};
    uint32_t address{0};
    uint32_t command{0};
    uint32_t rawValue{0};
    size_t bitCount{0};
    bool isRepeat{false};

    std::string protocolName() const;
    std::string toString() const;
};

class IrProtocol {
public:
    /**
     * @brief Corrects short leading AGC header mark if needed.
     * The IR receiver's AGC settling causes the first burst to measure shorter (e.g., ~6944 us instead of 9000 us).
     * If pulses[0] < 8000 us and space (abs(pulses[1])) is between 3500 and 5500 us, snaps pulses[0] to 9000 us.
     */
    static void fixLeadingMark(std::vector<int>& pulses, bool verbose = false);

    /**
     * @brief Normalizes a 32-bit NEC pulse sequence to standard 68 pulses at 38 kHz.
     * Snaps AGC leader mark (9000 us), leader space (-4500 us), standardizes data bits,
     * adds standard stop bit (560 us, -40000 us), and trims any trailing repeat frames or noise.
     * @return true if sequence was a valid NEC 32-bit frame and normalized to 68 pulses.
     */
    static bool normalizeNec(std::vector<int>& pulses, bool verbose = false);

    /**
     * @brief Encodes pulse timings (in microseconds) to Tiqiaa USB device payload bytes.
     * Each pulse duration is converted to 16 us units. Byte bit 7 (0x80) indicates state (ON/OFF).
     * Values > 127 units are split across multiple bytes.
     */
    static std::vector<uint8_t> encodeTiqiaaPayload(const std::vector<int>& pulses, uint8_t frequency_khz = 38);

    /**
     * @brief Decodes raw USB payload bytes received from Tiqiaa device into an IrSignal.
     * Locates 'D' marker and 'EN' terminator, converts 16 us units to microseconds,
     * merges consecutive matching state segments, filters out noise (< 50 us), and fixes AGC leader mark.
     * @return true if successfully decoded a valid signal, false otherwise.
     */
    static bool decodeTiqiaaPayload(const std::vector<uint8_t>& payload, IrSignal& outSignal, bool verbose = false);

    /**
     * @brief Parses string representation of pulse sequence (e.g. "9000, -4500, 560, -560").
     */
    static std::vector<int> parsePulseSequence(const std::string& inputStr);

    /**
     * @brief Formats pulse array to string representation.
     */
    static std::string formatPulseSequence(const std::vector<int>& pulses);

    /**
     * @brief Attempts to decode a NEC IR protocol signal.
     */
    static bool decodeNec(const std::vector<int>& pulses, DecodedCode& outCode);

    /**
     * @brief Encodes address and command into NEC protocol pulse sequence.
     */
    static std::vector<int> encodeNec(uint32_t address, uint32_t command);
};

} // namespace ir
