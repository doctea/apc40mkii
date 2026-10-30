#pragma once

#include "APC40Transport.h"
#include <RtMidi.h>
#include <string>
#include <vector>

class APC40RtMidiTransport : public APC40Transport {
public:
    explicit APC40RtMidiTransport(const std::string& portName = "USB Audio Device");
    ~APC40RtMidiTransport() override;

    bool open() override;
    void close() override;
    bool isConnected() const override;
    bool sendMessage(const uint8_t* data, std::size_t size) override;
    void poll() override;

private:
    std::string portName;
    RtMidiIn midiIn;
    RtMidiOut midiOut;
    std::vector<unsigned char> incoming;
    bool connected = false;
};