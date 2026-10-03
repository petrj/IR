#include "SignalStorage.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace ir {

static std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

std::string SavedIrSignal::toJsonString() const {
    std::ostringstream oss;
    oss << "    {\n";
    oss << "      \"name\": \"" << escapeJson(name) << "\",\n";
    oss << "      \"protocol\": \"" << escapeJson(protocol) << "\",\n";
    oss << "      \"frequency\": " << static_cast<int>(frequency_khz) << ",\n";
    oss << "      \"timestamp\": \"" << escapeJson(timestamp) << "\",\n";
    oss << "      \"rawHex\": \"" << escapeJson(rawHex) << "\",\n";
    oss << "      \"pulses\": [";
    for (size_t i = 0; i < pulses.size(); ++i) {
        oss << pulses[i];
        if (i + 1 < pulses.size()) {
            oss << ", ";
        }
    }
    oss << "]\n";
    oss << "    }";
    return oss.str();
}

SignalStorage::SignalStorage(std::string storagePath)
    : m_storagePath(std::move(storagePath)) {
    loadFromFile();
}

const std::string& SignalStorage::getStoragePath() const {
    return m_storagePath;
}

const std::vector<SavedIrSignal>& SignalStorage::getSignals() const {
    return m_signals;
}

std::optional<SavedIrSignal> SignalStorage::getSignal(size_t index) const {
    if (index < m_signals.size()) {
        return m_signals[index];
    }
    return std::nullopt;
}

size_t SignalStorage::count() const {
    return m_signals.size();
}

void SignalStorage::addSignal(const SavedIrSignal& signal) {
    m_signals.push_back(signal);
    saveToFile();
}

bool SignalStorage::removeSignal(size_t index) {
    if (index < m_signals.size()) {
        m_signals.erase(m_signals.begin() + index);
        saveToFile();
        return true;
    }
    return false;
}

void SignalStorage::clear() {
    m_signals.clear();
    saveToFile();
}

bool SignalStorage::saveToFile(const std::string& path) const {
    std::string targetPath = path.empty() ? m_storagePath : path;
    std::ofstream outFile(targetPath);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << "[\n";
    for (size_t i = 0; i < m_signals.size(); ++i) {
        outFile << m_signals[i].toJsonString();
        if (i + 1 < m_signals.size()) {
            outFile << ",";
        }
        outFile << "\n";
    }
    outFile << "]\n";

    return true;
}

static std::string extractJsonString(const std::string& block, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = block.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = block.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return "";

    size_t quoteStart = block.find('"', colonPos + 1);
    if (quoteStart == std::string::npos) return "";

    size_t quoteEnd = block.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";

    return block.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

static int extractJsonInt(const std::string& block, const std::string& key, int defaultVal = 0) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = block.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = block.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return defaultVal;

    size_t valStart = block.find_first_of("0123456789-", colonPos + 1);
    if (valStart == std::string::npos) return defaultVal;

    size_t valEnd = block.find_first_not_of("0123456789-", valStart);
    std::string valStr = block.substr(valStart, valEnd - valStart);
    try {
        return std::stoi(valStr);
    } catch (...) {
        return defaultVal;
    }
}

static std::vector<int> extractJsonPulses(const std::string& block) {
    std::string searchKey = "\"pulses\"";
    size_t keyPos = block.find(searchKey);
    if (keyPos == std::string::npos) return {};

    size_t openBracket = block.find('[', keyPos);
    if (openBracket == std::string::npos) return {};

    size_t closeBracket = block.find(']', openBracket);
    if (closeBracket == std::string::npos) return {};

    std::string content = block.substr(openBracket + 1, closeBracket - openBracket - 1);
    return IrProtocol::parsePulseSequence(content);
}

bool SignalStorage::loadFromFile(const std::string& path) {
    std::string targetPath = path.empty() ? m_storagePath : path;
    std::ifstream inFile(targetPath);
    if (!inFile.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string json = buffer.str();

    m_signals.clear();

    size_t pos = 0;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        size_t endPos = json.find('}', pos);
        if (endPos == std::string::npos) break;

        // Ensure we catch the full object even if pulses contains brackets
        size_t nextOpen = json.find('{', pos + 1);
        while (nextOpen != std::string::npos && nextOpen < endPos) {
            endPos = json.find('}', endPos + 1);
            if (endPos == std::string::npos) break;
            nextOpen = json.find('{', nextOpen + 1);
        }

        if (endPos == std::string::npos) break;

        std::string block = json.substr(pos, endPos - pos + 1);

        SavedIrSignal sig;
        sig.name = extractJsonString(block, "name");
        sig.protocol = extractJsonString(block, "protocol");
        sig.timestamp = extractJsonString(block, "timestamp");
        sig.rawHex = extractJsonString(block, "rawHex");
        sig.frequency_khz = static_cast<uint8_t>(extractJsonInt(block, "frequency", 38));
        sig.pulses = extractJsonPulses(block);

        if (!sig.pulses.empty() || !sig.name.empty()) {
            m_signals.push_back(std::move(sig));
        }

        pos = endPos + 1;
    }

    return true;
}

} // namespace ir
