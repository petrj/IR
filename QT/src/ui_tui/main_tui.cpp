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
                      << "  --send <pulses>     Transmit pulse sequence (e.g. \"9000, -4500, 560, -560\")\n"
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
        } else if (arg == "--send" && argc > 2) {
            std::string pulseStr = argv[2];
            auto pulses = ir::IrProtocol::parsePulseSequence(pulseStr);
            if (pulses.empty()) {
                std::cerr << "Invalid pulse sequence provided.\n";
                return 1;
            }
            if (!transceiver->openDevice()) {
                std::cerr << "Hardware device not available.\n";
                return 1;
            }
            return transceiver->transmitIr(pulses, 38) ? 0 : 1;
        }
    }

    // Default: run interactive console menu
    controller.runInteractiveLoop();
    return 0;
}
