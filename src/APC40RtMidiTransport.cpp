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
        bool inputFound = false;
        const unsigned int inputCount = midiIn.getPortCount();
        for (unsigned int index = 0; index < inputCount; ++index) {
            if (midiIn.getPortName(index).find(portName) != std::string::npos) {
                midiIn.openPort(index);
                inputFound = true;
                break;
            }
        }
        bool outputFound = false;
        const unsigned int outputCount = midiOut.getPortCount();
        for (unsigned int index = 0; index < outputCount; ++index) {
            if (midiOut.getPortName(index).find(portName) != std::string::npos) {
                midiOut.openPort(index);
                outputFound = true;
                break;
            }
        }
        if (!midiIn.isPortOpen() || !midiOut.isPortOpen()) {
            if (!inputFound) {
                std::cerr << "No APC40 MIDI input matching '" << portName << "'. Available inputs:";
                for (unsigned int index = 0; index < inputCount; ++index)
                    std::cerr << "\n  " << index << ": " << midiIn.getPortName(index);
                std::cerr << '\n';
            }
            if (!outputFound) {
                std::cerr << "No APC40 MIDI output matching '" << portName << "'. Available outputs:";
                for (unsigned int index = 0; index < outputCount; ++index)
                    std::cerr << "\n  " << index << ": " << midiOut.getPortName(index);
                std::cerr << '\n';
            }
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