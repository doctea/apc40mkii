#pragma once
#include <functional>
#include <cstdint>
#include "APC40Controller.h"

#include "APCControl.h"

#include "GenericList.h"

class APC40BankSet {
public:
    APC40BankSet(
        APC40Controller* controller,
        const char *name,
        int numBanks,
        const uint8_t bankButtons[],
        const char *const bankNames[],
        uint8_t baseCC,
        int numEncoders
    );

    void handleButton(uint8_t note, uint8_t vel, uint8_t channel);
    bool handleControlChange(uint8_t cc, uint8_t value, uint8_t channel);
    void setEncoderValue(int encoderIndex, int bank, uint8_t value);
    bool getEncoderState(int encoderIndex, int bank, APC40VirtualEncoderState& state) const;
    bool setEncoderState(int encoderIndex, int bank, const APC40VirtualEncoderState& state);
    bool setEncoderPolarity(int encoderIndex, int bank, APC40EncoderPolarity polarity);
    bool setEncoderRingMode(int encoderIndex, int bank, APC40EncoderRingMode ringMode);
    
    void setBank(int bank);
    int getCurrentBank() const { return currentBank; }

    const char *getName() const { return name; }
    const char *getCurrentBankName() const { return bankNames[currentBank]; }

    void addBank(const char *bankName, uint8_t bankNote, uint8_t baseCC, int numEncoders);

    // Callback: (encoderIndex, bankIndex, value)
    std::function<void(int, int, uint8_t)> onBankedChange;

    void redraw();

private:
    APC40Controller& controller;
    const char *name;

    int numBanks;
    const char* const* bankNames;

    GenericList<APCControl*> *bankButtons;
    GenericList<GenericList<APCControl*>*> *bankEncoders;

    uint8_t baseCC;
    int numEncoders;
    int currentBank = 0;

    APCControl* getEncoderControl(int encoderIndex, int bank) const;
    void redrawEncoder(int encoderIndex, int bank);
};
