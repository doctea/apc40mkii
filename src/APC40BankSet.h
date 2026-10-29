#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include "APC40Device.h"

class APC40BankSet {
public:
    APC40BankSet(
        APC40Device& dev,
        const std::string& name,
        int numBanks,
        const std::vector<uint8_t>& bankButtons,
        const std::vector<std::string>& bankNames,
        uint8_t baseCC,
        int numEncoders
    );

    void handleButton(uint8_t note, uint8_t vel, uint8_t channel);
    void handleControlChange(uint8_t cc, uint8_t value, uint8_t channel);

    int getCurrentBank() const { return currentBank; }
    const std::string& getName() const { return name; }
    const std::string& getCurrentBankName() const { return bankNames[currentBank]; }

    // Callback: (encoderIndex, bankIndex, value)
    std::function<void(int, int, uint8_t)> onBankedChange;

    void redraw();

private:
    APC40Device& device;
    std::string name;

    int numBanks;
    std::vector<uint8_t> bankButtons;
    std::vector<std::string> bankNames;

    std::vector<std::vector<uint8_t>> bankValues; // Stores the values for each bank

    uint8_t baseCC;
    int numEncoders;

    int currentBank = 0;

    void setBank(int bank);
};
