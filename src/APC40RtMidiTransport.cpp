#include "APC40RtMidiTransport.h"
#include <iostream>

APC40RtMidiTransport::APC40RtMidiTransport(const std::string& portName)
    : portName(portName), midiIn(RtMidi::UNSPECIFIED, "APC40 input", 256) {
}

APC40RtMidiTransport::~APC40RtMidiTransport() {
    close();
}

bool APC40RtMidiTransport::open() {
    if (isConnected())
        return true;

    close();
    try {
        for (unsigned int count = 0; count < 256; ++count) {
            midiIn.getMessage(&incoming);
            if (incoming.empty())
                break;
        }
        incoming.clear();
        for (unsigned int index = 0; index < midiIn.getPortCount(); ++index) {
            if (midiIn.getPortName(index).find(portName) != std::string::npos) {
                midiIn.openPort(index);
                break;
            }
        }
        for (unsigned int index = 0; index < midiOut.getPortCount(); ++index) {
            if (midiOut.getPortName(index).find(portName) != std::string::npos) {
                midiOut.openPort(index);
                break;
            }
        }
        if (!midiIn.isPortOpen() || !midiOut.isPortOpen()) {
            close();
            return false;
        }
        midiIn.ignoreTypes(false, true, true);
        connected = true;
    } catch (const RtMidiError& error) {
        std::cerr << "APC40 MIDI open failed: " << error.getMessage() << '\n';
        close();
        return false;
    }

    if (onConnectionChanged)
        onConnectionChanged(true);
    return true;
}

void APC40RtMidiTransport::close() {
    const bool wasConnected = connected;
    connected = false;
    midiIn.closePort();
    midiOut.closePort();
    incoming.clear();
    if (wasConnected && onConnectionChanged)
        onConnectionChanged(false);
}

bool APC40RtMidiTransport::isConnected() const {
    return connected && midiIn.isPortOpen() && midiOut.isPortOpen();
}

bool APC40RtMidiTransport::sendMessage(const uint8_t* data, std::size_t size) {
    if (!isConnected() || !data || size == 0)
        return false;
    try {
        midiOut.sendMessage(data, size);
        return true;
    } catch (const RtMidiError& error) {
        std::cerr << "APC40 MIDI send failed: " << error.getMessage() << '\n';
        close();
        return false;
    }
}

void APC40RtMidiTransport::poll() {
    if (!isConnected()) {
        if (connected)
            close();
        return;
    }

    try {
        for (unsigned int count = 0; count < 64 && isConnected(); ++count) {
            midiIn.getMessage(&incoming);
            if (incoming.empty())
                break;
            if (onMessage)
                onMessage(incoming.data(), incoming.size());
        }
    } catch (const RtMidiError& error) {
        std::cerr << "APC40 MIDI receive failed: " << error.getMessage() << '\n';
        close();
    }
}