#include "TuiController.h"
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string apiUrl = "http://127.0.0.1:8080";
    const char* envUrl = std::getenv("IR_API_URL");
    if (envUrl != nullptr && *envUrl != '\0') {
        apiUrl = envUrl;
    }

    // Parse options
    int cmdArgIdx = -1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--api" || arg == "-a") {
            if (i + 1 < argc) {
                apiUrl = argv[++i];
            } else {
                std::cerr << "[ERROR] --api option requires a URL argument.\n";
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ir_tui [OPTIONS]\n"
                      << "Options:\n"
                      << "  --api, -a <url>     Set API server base URL (default: " << apiUrl << ")\n"
                      << "  --read, -r          Capture a single IR signal via API and exit\n"
                      << "  --trigger, -t       Start Trigger Mode (listen and execute shell commands)\n"
                      << "  --list, -l          List all saved IR signals\n"
                      << "  --send, -s [arg]    Transmit IR signal via API (interactive, index, name, or pulses)\n"
                      << "  --info, -i          Print API & hardware status diagnostics\n"
                      << "  --help, -h          Show this help message\n"
                      << "\nEnvironment Variables:\n"
                      << "  IR_API_URL          API server base URL (overridden by --api flag)\n"
                      << "\nRun without arguments to start the interactive console menu.\n";
            return 0;
        } else if (cmdArgIdx == -1) {
            cmdArgIdx = i;
        }
    }

    ir::TuiController controller(apiUrl);

    if (cmdArgIdx != -1) {
        std::string arg = argv[cmdArgIdx];
        if (arg == "--read" || arg == "-r") {
            return controller.captureOnce(10.0f) ? 0 : 1;
        } else if (arg == "--trigger" || arg == "-t") {
            controller.startTriggerMode();
            return 0;
        } else if (arg == "--list" || arg == "-l") {
            controller.listSavedSignals();
            return 0;
        } else if (arg == "--info" || arg == "-i") {
            controller.printHardwareInfo();
            return 0;
        } else if (arg == "--send" || arg == "-s") {
            if (cmdArgIdx + 1 < argc && std::string(argv[cmdArgIdx + 1]).rfind("-", 0) != 0) {
                std::string target = argv[cmdArgIdx + 1];
                auto pulses = ir::IrProtocol::parsePulseSequence(target);
                uint8_t freq = 38;
                std::string sigName = "Pulse Sequence";
                if (pulses.empty()) {
                    const auto& list = controller.getStorage().getSignals();
                    bool found = false;
                    try {
                        size_t idx = std::stoul(target);
                        if (idx >= 1 && idx <= list.size()) {
                            pulses = list[idx - 1].pulses;
                            freq = list[idx - 1].frequency_khz;
                            sigName = list[idx - 1].name;
                            found = true;
                        }
                    } catch (...) {}

                    if (!found) {
                        for (const auto& s : list) {
                            if (s.name == target) {
                                pulses = s.pulses;
                                freq = s.frequency_khz;
                                sigName = s.name;
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found) {
                        std::cerr << "[ERROR] '" << target << "' is not a valid pulse sequence or saved signal.\n";
                        controller.listSavedSignals();
                        return 1;
                    }
                }

                return controller.transmitPulses(pulses, freq, sigName) ? 0 : 1;
            } else {
                controller.transmitSavedSignal();
                return 0;
            }
        } else {
            std::cerr << "[ERROR] Unknown argument: " << arg << "\n";
            std::cerr << "Use --help to see available options.\n";
            return 1;
        }
    }

    // Default: run interactive console menu
    controller.runInteractiveLoop();
    return 0;
}
