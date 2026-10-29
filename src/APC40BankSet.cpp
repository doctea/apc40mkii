#include "APC40BankSet.h"
#include <iostream>

APC40BankSet::APC40BankSet(
    APC40Device& dev,
    const std::string& name,
    int numBanks,
    const std::vector<uint8_t>& bankButtons,
    const std::vector<std::string>& bankNames,
    uint8_t baseCC,
    int numEncoders
)
    : device(dev),
      name(name),
      numBanks(numBanks),
      bankButtons(bankButtons),
      bankNames(bankNames),
      baseCC(baseCC),
      numEncoders(numEncoders),
      currentBank(0)
{
    if (bankNames.size() != numBanks) {
        std::cerr << "APC40BankSet: bankNames size mismatch\n";
    }
    bankValues.resize(numBanks);
    for (int i = 0; i < numBanks; i++) {
        bankValues[i].resize(numEncoders, 0);
    }
}

void APC40BankSet::setBank(int bank) {
    if (bank < 0 || bank >= numBanks)
        return;

    currentBank = bank;

    // LED feedback
    for (int i = 0; i < numBanks; i++) {
        uint8_t btn = bankButtons[i];
        uint8_t colour = (i == currentBank) ? 3 : 0; // green / off
        device.sendNoteOn(btn, 0, colour);
    }

    redraw();
}

void APC40BankSet::handleButton(uint8_t note, uint8_t vel, uint8_t channel) {
    if (vel == 0) return;

    for (int i = 0; i < numBanks; i++) {
        if (bankButtons[i] == note) {
            setBank(i);
            return;
        }
    }
}

void APC40BankSet::handleControlChange(uint8_t cc, uint8_t value, uint8_t channel) {
    int idx = cc - baseCC;
    if (idx < 0 || idx >= numEncoders)
        return;

    // set the value for the current bank and encoder
    bankValues[currentBank][idx] = value;

    if (onBankedChange)
        onBankedChange(idx, currentBank, value);
}

void APC40BankSet::redraw() {
    // Simple ring feedback: show bank index visually
    for (int i = 0; i < numEncoders; i++) {
        uint8_t cc = baseCC + i;
        uint8_t ringValue = bankValues[currentBank][i];
        device.sendCC(cc, 0, ringValue);
    }
}
