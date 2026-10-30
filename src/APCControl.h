#pragma once
#include <functional>
#include <string>
#include <cstdint>

enum class APC40EncoderPolarity {
    Unipolar,
    Bipolar
};

enum class APC40EncoderRingMode {
    DeviceDefault,
    Automatic,
    Off,
    Single,
    Volume,
    Pan
};

enum class APCControlType {
    Button,
    Encoder
};

struct APC40VirtualEncoderState {
    uint8_t value = 0;
    APC40EncoderPolarity polarity = APC40EncoderPolarity::Unipolar;
    APC40EncoderRingMode ringMode = APC40EncoderRingMode::DeviceDefault;
};

class APCControl {
public:

    APCControl() = default;
    APCControl(APCControlType type, uint8_t number, uint8_t channel, const char* name, bool hasLED = false)
        : APCControl() {
        this->type = type;
        this->name = name;
        this->hasLED = hasLED;
        this->midiChannel = channel;
        if (type == APCControlType::Button)
            this->midiNote = number;
        else if (type == APCControlType::Encoder)
            this->midiCC = number;
    }

    APCControlType type;
    const char* name;

    uint8_t midiNote = 0;
    uint8_t midiCC = 0;
    uint8_t midiChannel = 0;

    bool hasLED = false;
    uint8_t ledOn = 3;   // green
    uint8_t ledOff = 0;

    float value = 0.0f;
    APC40EncoderPolarity polarity = APC40EncoderPolarity::Unipolar;
    APC40EncoderRingMode ringMode = APC40EncoderRingMode::DeviceDefault;

    // Callbacks
    std::function<void(float)> onAppValueChanged;   // App → APC
    std::function<void(float)> onAPCValueChanged;   // APC → App
    std::function<float()> pullFromApp;             // APC → App (pull)

    void applyAPCEvent(float v);                    // APC → App
    void setAppValue(float v);                      // App → APC
};
