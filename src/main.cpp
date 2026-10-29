#include "APC40Controller.h"
#include <iostream>
#include <thread>
#include <chrono>

#include "constants.h"

int main() {
    APC40Controller apc;

    if (!apc.init()) {
        std::cerr << "Failed to initialise APC40 MkII" << std::endl;
        return 1;
    }

    std::cout << "APC40 MkII connected." << std::endl;

    // switch to Mode 1
    const uint8_t message[] = {
        APC40MKII::sysex::APPLICATION_MODES::MODE_1, // mode
        0x09, // major version
        0x07, // minor version
        0x01  // bugfix level
    };
    apc.device.send_sysex_command(
        APC40MKII::sysex::MESSAGE_TYPE_INTRODUCTION_IDENTIFIER,
        4,
        message
    );

    // Example: turn on LED for button note 64 (just an example)
    apc.feedback.setButtonLED(64, 1, 3); // channel 0, velocity 3 = green on APC40 MkII

    while (true) {
        apc.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
