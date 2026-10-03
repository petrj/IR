#include "TuiController.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    auto transceiver = std::make_shared<ir::IrUsbTransceiver>(38);
    ir::TuiController controller(transceiver);

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ir_tui_test [OPTIONS]\n"
                      << "Options:\n"
                      << "  --read, -r          Capture a single IR signal and exit\n"
                      << "  --list, -l          List all saved IR signals\n"
                      << "  --send, -s [arg]    Transmit IR signal (offers saved signals, or specify pulses/name)\n"
                      << "  --info, -i          Print USB hardware diagnostics\n"
                      << "  --help, -h          Show this help message\n"
                      << "\nRun without arguments to start the interactive console menu.\n";
            return 0;
        } else if (arg == "--read" || arg == "-r") {
            return controller.captureOnce(10.0f) ? 0 : 1;
        } else if (arg == "--list" || arg == "-l") {
            controller.listSavedSignals();
            return 0;
        } else if (arg == "--info" || arg == "-i") {
            controller.printHardwareInfo();
            return 0;
        } else if (arg == "--send" || arg == "-s") {
            if (argc > 2) {
                std::string target = argv[2];
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

                if (!transceiver->openDevice()) {
                    std::cerr << "[ERROR] Hardware device not available.\n";
                    return 1;
                }
                std::cout << "[INFO] Transmitting '" << sigName << "' (" << pulses.size() << " pulses)...\n";
                return transceiver->transmitIr(pulses, freq) ? 0 : 1;
            } else {
                controller.transmitSavedSignal();
                return 0;
            }
        }
    }

    // Default: run interactive console menu
    controller.runInteractiveLoop();
    return 0;
}
