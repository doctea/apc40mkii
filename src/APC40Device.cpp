#include "APC40Device.h"

APC40Device::APC40Device(APC40Transport& transport) : transport(transport) {
    transport.onMessage = [this](const uint8_t* data, std::size_t size) {
        return receiveMessage(data, size);
    };
    transport.onConnectionChanged = [this](bool connected) {
        if (onConnectionChanged)
            onConnectionChanged(connected);
    };
}

APC40Device::~APC40Device() {
    transport.onMessage = nullptr;
    transport.onConnectionChanged = nullptr;
}

bool APC40Device::open() {
    return transport.open();
}

bool APC40Device::isConnected() const {
    return transport.isConnected();
}

void APC40Device::loop() {
    transport.poll();
}

bool APC40Device::receiveMessage(const uint8_t* data, std::size_t size) {
    if (!data || size == 0)
        return false;

    if (data[0] == 0xF0) {
        if (size >= 2 && data[size - 1] == 0xF7 && onSystemExclusive) {
            onSystemExclusive(data, size);
            return true;
        }
        return false;
    }

    if (size != 3 || data[1] > 127 || data[2] > 127)
        return false;

    const uint8_t status = data[0] & 0xF0;
    const uint8_t channel = data[0] & 0x0F;
    if (status == 0x80 || (status == 0x90 && data[2] == 0)) {
        if (onNoteOff) {
            onNoteOff(data[1], channel, data[2]);
            return true;
        }
    } else if (status == 0x90) {
        if (onNoteOn) {
            onNoteOn(data[1], channel, data[2]);
            return true;
        }
    } else if (status == 0xB0) {
        const bool handled = handleControlChange && handleControlChange(data[1], channel, data[2]);
        if (onControlChange)
            onControlChange(data[1], channel, data[2]);
        return handled;
    }
    return false;
}

void APC40Device::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (!isConnected() || note > 127 || velocity > 127 || channel > 15)
        return;
    const uint8_t message[] = {static_cast<uint8_t>(0x90 | channel), note, velocity};
    transport.sendMessage(message, sizeof(message));
}

void APC40Device::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (!isConnected() || note > 127 || velocity > 127 || channel > 15)
        return;
    const uint8_t message[] = {static_cast<uint8_t>(0x80 | channel), note, velocity};
    transport.sendMessage(message, sizeof(message));
}

void APC40Device::sendCC(uint8_t cc, uint8_t value, uint8_t channel) {
    if (!isConnected() || cc > 127 || value > 127 || channel > 15)
        return;
    const uint8_t message[] = {static_cast<uint8_t>(0xB0 | channel), cc, value};
    transport.sendMessage(message, sizeof(message));
}

void APC40Device::send_sysex_command(uint8_t message_identifier, int16_t size, const uint8_t* data) {
    if (!isConnected() || message_identifier > 127 || size < 0 || size > 248 || (size > 0 && !data))
        return;

    uint8_t message[256] = {0xF0, 0x47, 0x7F, 0x29, message_identifier,
                           static_cast<uint8_t>((size >> 7) & 0x7F),
                           static_cast<uint8_t>(size & 0x7F)};
    for (int index = 0; index < size; ++index) {
        if (data[index] > 127)
            return;
        message[7 + index] = data[index];
    }
    message[7 + size] = 0xF7;
    transport.sendMessage(message, static_cast<std::size_t>(size) + 8);
}
