#include "APC40Feedback.h"

void APC40Feedback::setButtonLED(uint8_t note, uint8_t channel, uint8_t colour) {
    device.sendNoteOn(note, channel, colour);
}

void APC40Feedback::setEncoderRing(uint8_t cc, uint8_t channel, uint8_t value) {
    device.sendCC(cc, channel, value);
}
