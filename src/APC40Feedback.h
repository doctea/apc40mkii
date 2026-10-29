#pragma once
#include "APC40Device.h"

class APC40Feedback {
public:
    APC40Feedback(APC40Device& dev) : device(dev) {}

    void setButtonLED(uint8_t note, uint8_t channel, uint8_t colour);
    void setEncoderRing(uint8_t cc, uint8_t channel, uint8_t value);

private:
    APC40Device& device;
};
