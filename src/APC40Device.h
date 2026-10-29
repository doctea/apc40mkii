#pragma once
#include <functional>
#include <string>
#include <RtMidi.h>
#include <cstdint>

class APC40Device {
public:
    bool open();
    void sendNoteOn(uint8_t note, uint8_t channel, uint8_t velocity);
    void sendNoteOff(uint8_t note, uint8_t channel, uint8_t velocity);
    void sendCC(uint8_t cc, uint8_t channel, uint8_t value);

    std::function<void(uint8_t, uint8_t, uint8_t)> onNoteOn;
    std::function<void(uint8_t, uint8_t, uint8_t)> onNoteOff;
    std::function<void(uint8_t, uint8_t, uint8_t)> onControlChange;

    virtual void send_sysex_command(uint8_t message_identifier, const int16_t size, const uint8_t *data) {
        static uint8_t *sysex = new uint8_t[256];
        sysex[0] = 0xF0;
        sysex[1] = 0x47;
        sysex[2] = 0x7F;
        sysex[3] = 0x29;
        sysex[4] = message_identifier;
        sysex[5] = size >> 8; // MSB of size
        sysex[6] = size & 0xFF; // LSB of size
        for (int i = 0; i < size; i++) {
            sysex[7 + i] = data[i];
        }
        sysex[size + 7] = 0xF7;
        this->midiOut.sendMessage(&sysex[0], size + 8);
    }

private:
    RtMidiIn midiIn;
    RtMidiOut midiOut;
};
