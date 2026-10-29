#include "APC40Controller.h"
#include <iostream>
#include "APC40Controller.h"

APC40Controller::APC40Controller()
    : device(),
      feedback(device)
{
}

bool APC40Controller::init() {
    if (!device.open()) {
        std::cerr << "Could not open APC40 MkII" << std::endl;
        return false;
    }

    device.onNoteOn = [&](uint8_t note, uint8_t channel, uint8_t vel) {
        std::cout << "NOTE: " << (int)note << " ch=" << (int)channel+1 << " vel=" << (int)vel << std::endl;
        for (auto* bs : bank_sets)
            bs->handleButton(note, vel, channel);
    };

    device.onNoteOff = [&](uint8_t note, uint8_t channel, uint8_t vel) {
        std::cout << "NOTE OFF: " << (int)note << " ch=" << (int)channel+1 << " vel=" << (int)vel << std::endl;
    };

    device.onControlChange = [&](uint8_t cc, uint8_t channel, uint8_t val) {
        std::cout << "CC: " << (int)cc << " ch=" << (int)channel+1 << " val=" << (int)val << std::endl;
        for (auto* bs : bank_sets)
            bs->handleControlChange(cc, val, channel);
    };

    setup_banks();

    return true;
}

void APC40Controller::poll() {
    // RtMidi is callback-driven, so nothing needed here yet.
}


void APC40Controller::setup_banks() {

    bank_sets[0] = new APC40BankSet(
        device,
        "Track Knobs",
        3,
        { 
            APC40MKII_BUTTON_PAN,
            APC40MKII_BUTTON_SENDS,
            APC40MKII_BUTTON_USER 
        },
        {
            "Pan",
            "Sends",
            "User"
        },
        APC40MKII_KNOB_TOP_CC_1,
        8
    );

    bank_sets[1] = new APC40BankSet(
        device,
        "Device Knobs",
        8, // or 8, or 16 — your choice
        { 
            APC40MKII_BUTTON_DEVICE_1,
            APC40MKII_BUTTON_DEVICE_2,
            APC40MKII_BUTTON_DEVICE_3,
            APC40MKII_BUTTON_DEVICE_4,
            APC40MKII_BUTTON_DEVICE_5,
            APC40MKII_BUTTON_DEVICE_6,
            APC40MKII_BUTTON_DEVICE_7,
            APC40MKII_BUTTON_DEVICE_8
        },
        {
            "Device 1",
            "Device 2",
            "Device 3",
            "Device 4",
            "Device 5",
            "Device 6",
            "Device 7",
            "Device 8"
        },
        APC40MKII_DEVICE_KNOB_CC_1,
        8
    );

    bank_sets[0]->onBankedChange = [&](int idx, int bank, uint8_t val) {
        // Track knobs → sequencer
    };

    bank_sets[1]->onBankedChange = [&](int idx, int bank, uint8_t val) {
        // Device knobs → synth parameters
    };

    // new APC40Banks(device, "Device Knobs", 8, APC40MKII_DEVICE_KNOB_CC_1, 8)
}