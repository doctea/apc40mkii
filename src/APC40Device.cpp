#include "APC40Device.h"
#include <iostream>

bool APC40Device::open() {
    bool openedIn = false;
    bool openedOut = false;

    unsigned int nIn = midiIn.getPortCount();
    for (unsigned int i = 0; i < nIn; i++) {
        auto name = midiIn.getPortName(i);
        if (name.find("USB Audio Device") != std::string::npos) {
            midiIn.openPort(i);
            openedIn = true;
            break;
        }
    }

    unsigned int nOut = midiOut.getPortCount();
    for (unsigned int i = 0; i < nOut; i++) {
        auto name = midiOut.getPortName(i);
        if (name.find("USB Audio Device") != std::string::npos) {
            midiOut.openPort(i);
            openedOut = true;
            break;
        }
    }

    if (!openedIn || !openedOut) {
        std::cerr << "Could not find APC40 MkII ports\n";
        return false;
    }


    midiIn.setCallback([](double, std::vector<unsigned char>* msg, void* userData) {
        auto* self = static_cast<APC40Device*>(userData);
        uint8_t status = (*msg)[0] & 0xF0;

        if (status == 0x90) {
            if (self->onNoteOn) self->onNoteOn((*msg)[1], (*msg)[0] & 0x0F, (*msg)[2]);
        } else if (status == 0x80) {
            if (self->onNoteOff) self->onNoteOff((*msg)[1], (*msg)[0] & 0x0F, (*msg)[2]);
        } else if (status == 0xB0) {
            if (self->onControlChange) self->onControlChange((*msg)[1], (*msg)[0] & 0x0F, (*msg)[2]);
        }
    }, this);

    midiIn.ignoreTypes(false, false, false);

    return true;
}

void APC40Device::sendNoteOn(uint8_t note, uint8_t channel, uint8_t velocity) {
    std::vector<unsigned char> msg = {static_cast<uint8_t>(0x90 | (channel & 0x0F)), note, velocity};
    midiOut.sendMessage(&msg);
}

void APC40Device::sendNoteOff(uint8_t note, uint8_t channel, uint8_t velocity) {
    std::vector<unsigned char> msg = {static_cast<uint8_t>(0x80 | (channel & 0x0F)), note, velocity};
    midiOut.sendMessage(&msg);
}

void APC40Device::sendCC(uint8_t cc, uint8_t channel, uint8_t value) {
    std::vector<unsigned char> msg = {static_cast<uint8_t>(0xB0 | (channel & 0x0F)), cc, value};
    midiOut.sendMessage(&msg);
}
