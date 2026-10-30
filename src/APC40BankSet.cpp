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
    controller.device.sendCC(encoder->midiCC, value, encoder->midiChannel);

    if (onBankedChange)
        onBankedChange(idx, currentBank, value);
}

void APC40BankSet::setEncoderValue(int encoderIndex, int bank, uint8_t value) {
    if (bank < 0 || bank >= numBanks || encoderIndex < 0 || encoderIndex >= numEncoders)
        return;

    if (value > 127)
        value = 127;

    APCControl* encoder = (*(*bankEncoders)[bank])[encoderIndex];
    encoder->value = value;

    if (bank == currentBank)
        controller.device.sendCC(encoder->midiCC, value, encoder->midiChannel);
}

void APC40BankSet::redraw() {
    // Simple ring feedback: show bank index visually
    for (int i = 0; i < numEncoders; i++) {
        GenericList<APCControl*> *encoders = (*bankEncoders)[currentBank];
        uint8_t cc = baseCC + i;
        uint8_t ringValue = (uint8_t) (*encoders)[i]->value;
        controller.device.sendCC(cc, ringValue, 0);
    }
}
