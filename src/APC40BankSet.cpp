#include "APC40BankSet.h"

APC40BankSet::APC40BankSet(
    APC40Controller* controller,
    const char *name,
    int numBanks,
    const uint8_t *bankButtons,
    const char *const *bankNames,
    uint8_t baseCC,
    int numEncoders
)
    : controller(*controller),
      name(name),
      numBanks(numBanks),
      bankNames(bankNames),
      baseCC(baseCC),
      numEncoders(numEncoders),
      currentBank(0)
{
    this->bankButtons = new GenericList<APCControl*>();
    this->bankEncoders = new GenericList<GenericList<APCControl*>*>();

    for (int i = 0; i < numBanks; i++) {
        this->addBank(bankNames[i], bankButtons[i], baseCC, numEncoders);
    }

}

void APC40BankSet::addBank(const char *bankName, uint8_t bankNote, uint8_t baseCC, int numEncoders) {

    // get (or create) the button to activate this bank
    APCControl* bankButton = this->controller.find_control(bankNote, 0, APCControlType::Button);

    if (!bankButton) {
        bankButton = new APCControl();
        bankButton->midiNote = bankNote;
        bankButton->midiChannel = 0; // default channel
        bankButton->type = APCControlType::Button;
        bankButton->value = 0; // initialize the value to 0
        this->controller.controls.add(bankButton);
    }

    bankButtons->add(bankButton);

    GenericList<APCControl*> *encoders = new GenericList<APCControl*>();

    // add controls for each encoder
    for (int i = 0; i < numEncoders; i++) {
        APCControl* encoder = new APCControl(APCControlType::Encoder, baseCC + i, 0, bankName, true);

        encoders->add(encoder);
    }
    bankEncoders->add(encoders);
}

void APC40BankSet::setBank(int bank) {
    if (bank < 0 || bank >= numBanks)
        return;

    currentBank = bank;

    // LED feedback
    for (auto* bank_button : *bankButtons) {
        uint8_t btn = bank_button->midiNote;
        uint8_t colour = (bank_button == (*bankButtons)[currentBank]) ? 1 : 0;
        controller.device.sendNoteOn(btn, colour, 0);
    }

    redraw();
}

void APC40BankSet::handleButton(uint8_t note, uint8_t vel, uint8_t channel) {
    if (vel == 0) return;

    for (int i = 0; i < numBanks; i++) {
        APCControl* button = (*bankButtons)[i];
        if (button->midiNote == note && button->midiChannel == channel) {
            setBank(i);
            return;
        }
    }
}

void APC40BankSet::handleControlChange(uint8_t cc, uint8_t value, uint8_t channel) {
    if (channel != 0)
        return;

    int idx = cc - baseCC;
    if (idx < 0 || idx >= numEncoders)
        return;

    // set the value for the current bank and encoder
    GenericList<APCControl*> *encoders = (*bankEncoders)[currentBank];

    APCControl *encoder = (*encoders)[idx];
    encoder->value = value;

    if (onBankedChange)
        onBankedChange(idx, currentBank, value);
}

void APC40BankSet::setEncoderValue(int encoderIndex, int bank, uint8_t value) {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder)
        return;

    if (value > 127)
        value = 127;
    encoder->value = value;

    if (bank == currentBank)
        controller.device.sendCC(encoder->midiCC, value, encoder->midiChannel);
}

APCControl* APC40BankSet::getEncoderControl(int encoderIndex, int bank) const {
    if (bank < 0 || bank >= numBanks || encoderIndex < 0 || encoderIndex >= numEncoders)
        return nullptr;
    return (*(*bankEncoders)[bank])[encoderIndex];
}

bool APC40BankSet::getEncoderState(int encoderIndex, int bank, APC40VirtualEncoderState& state) const {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder)
        return false;
    state.value = static_cast<uint8_t>(encoder->value);
    state.polarity = encoder->polarity;
    state.ringMode = encoder->ringMode;
    return true;
}

bool APC40BankSet::setEncoderState(int encoderIndex, int bank, const APC40VirtualEncoderState& state) {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder || state.value > 127)
        return false;
    switch (state.ringMode) {
        case APC40EncoderRingMode::Automatic:
        case APC40EncoderRingMode::DeviceDefault:
        case APC40EncoderRingMode::Off:
        case APC40EncoderRingMode::Single:
        case APC40EncoderRingMode::Volume:
        case APC40EncoderRingMode::Pan:
            break;
        default:
            return false;
    }
    if (state.polarity != APC40EncoderPolarity::Unipolar &&
        state.polarity != APC40EncoderPolarity::Bipolar)
        return false;

    encoder->value = state.value;
    encoder->polarity = state.polarity;
    encoder->ringMode = state.ringMode;
    if (bank == currentBank)
        redrawEncoder(encoderIndex, bank);
    return true;
}

bool APC40BankSet::setEncoderPolarity(int encoderIndex, int bank, APC40EncoderPolarity polarity) {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder || (polarity != APC40EncoderPolarity::Unipolar &&
        polarity != APC40EncoderPolarity::Bipolar))
        return false;
    encoder->polarity = polarity;
    if (bank == currentBank && encoder->ringMode != APC40EncoderRingMode::DeviceDefault)
        redrawEncoder(encoderIndex, bank);
    return true;
}

bool APC40BankSet::setEncoderRingMode(int encoderIndex, int bank, APC40EncoderRingMode ringMode) {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder)
        return false;
    switch (ringMode) {
        case APC40EncoderRingMode::Automatic:
        case APC40EncoderRingMode::DeviceDefault:
        case APC40EncoderRingMode::Off:
        case APC40EncoderRingMode::Single:
        case APC40EncoderRingMode::Volume:
        case APC40EncoderRingMode::Pan:
            break;
        default:
            return false;
    }
    encoder->ringMode = ringMode;
    if (bank == currentBank)
        redrawEncoder(encoderIndex, bank);
    return true;
}

void APC40BankSet::redrawEncoder(int encoderIndex, int bank) {
    APCControl* encoder = getEncoderControl(encoderIndex, bank);
    if (!encoder)
        return;

    if (encoder->ringMode == APC40EncoderRingMode::DeviceDefault) {
        controller.device.sendCC(encoder->midiCC,
            static_cast<uint8_t>(encoder->value), encoder->midiChannel);
        return;
    }

    APC40EncoderRingMode ringMode = encoder->ringMode;
    if (ringMode == APC40EncoderRingMode::Automatic)
        ringMode = encoder->polarity == APC40EncoderPolarity::Bipolar
            ? APC40EncoderRingMode::Pan : APC40EncoderRingMode::Volume;

    uint8_t modeValue = 1;
    switch (ringMode) {
        case APC40EncoderRingMode::Off:
            modeValue = 0;
            break;
        case APC40EncoderRingMode::Single:
            modeValue = 1;
            break;
        case APC40EncoderRingMode::Volume:
            modeValue = 2;
            break;
        case APC40EncoderRingMode::Pan:
            modeValue = 3;
            break;
        case APC40EncoderRingMode::DeviceDefault:
            break;
        case APC40EncoderRingMode::Automatic:
            break;
    }

    controller.device.sendCC(static_cast<uint8_t>(baseCC + 8 + encoderIndex),
        modeValue, encoder->midiChannel);
    controller.device.sendCC(encoder->midiCC,
        static_cast<uint8_t>(encoder->value), encoder->midiChannel);
}

void APC40BankSet::redraw() {
    for (int i = 0; i < numEncoders; i++) {
        redrawEncoder(i, currentBank);
    }
}
