#pragma once
#include "APC40Transport.h"

class APC40Device {
public:
    explicit APC40Device(APC40Transport& transport);
    virtual ~APC40Device();
    APC40Device(const APC40Device&) = delete;
    APC40Device& operator=(const APC40Device&) = delete;

    bool open();
    bool isConnected() const;
    void loop();
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendCC(uint8_t cc, uint8_t value, uint8_t channel);

    std::function<void(uint8_t, uint8_t, uint8_t)> onNoteOn;
    std::function<void(uint8_t, uint8_t, uint8_t)> onNoteOff;
    std::function<void(uint8_t, uint8_t, uint8_t)> onControlChange;
    std::function<void(const uint8_t*, std::size_t)> onSystemExclusive;
    std::function<void(bool)> onConnectionChanged;

    virtual void send_sysex_command(uint8_t message_identifier, int16_t size, const uint8_t* data);

private:
    APC40Transport& transport;
    void receiveMessage(const uint8_t* data, std::size_t size);
};
