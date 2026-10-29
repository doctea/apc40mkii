// #pragma once

// #include <functional>
// #include <cstdint>
// #include <cstring>
// #include <iostream>
// using namespace std;

// #include "APC40Device.h"

// class APC40Bank {
// public:

//     void updateValue(uint8_t ccIndex, uint8_t value) {
//         if (ccIndex < numControls) {
//             cout << "Updating control " << (int)ccIndex << " " << label << " to value " << (int)value << endl;
//             this->values[ccIndex] = value;
//         }
//     }

// public:
//     uint8_t numControls = 8;
//     uint8_t *values = nullptr;
//     uint8_t ccStartFrom = 0;
//     uint8_t *output_ccs = nullptr;

//     char label[40] = "Unknown Bank";

//     APC40Bank(const char *label, uint8_t numControls) {
//         strncpy(this->label, label, sizeof(this->label) - 1);
//         this->label[sizeof(this->label) - 1] = '\0';

//         this->numControls = numControls;
//         this->values = new uint8_t[numControls];

//         for (int i = 0; i < numControls; i++) {
//             this->values[i] = 0;
//         }
//     }

//     ~APC40Bank() {
//         delete[] this->values;
//     }

// };

// class APC40Banks {
// public:
//     APC40Banks(
//         APC40Device &device, 
//         const char *label, 
//         uint8_t numBanks, 
//         uint8_t bank_buttons[], 
//         uint8_t ccStartFrom, 
//         uint8_t numControls
//     ) {
//         this->device = &device;
//         strncpy(this->label, label, sizeof(this->label) - 1);
//         this->label[sizeof(this->label) - 1] = '\0';

//         this->numBanks = numBanks;
//         this->ccStartFrom = ccStartFrom;
//         this->numControls = numControls;
//         for (int i = 0; i < numBanks; i++) {
//             this->bank_buttons[i] = bank_buttons[i];
//         }
//     }

//     void setBank(int bank) { 
//         currentBank = bank;

//         cout << "Setting bank to " << bank << endl;

//         refresh();
//     }

//     void refresh() {
//         cout << "Refreshing bank " << currentBank << " " << label << endl;
//         // update the bank buttons
//         for (int i = 0; i < numBanks; i++) {
//             if (bank_buttons[i]) {
//                 uint8_t note = bank_buttons[i];
//                 if (i == currentBank) {
//                     device->sendNoteOn(note, 127, 0); // assuming channel 0 for now
//                 } else {
//                     device->sendNoteOff(note, 0, 0); // assuming channel 0 for now
//                 }
//             }
//         }

//         // update the hardware with the newly selected bank's control values
//         for (int i = 0; i < banks[currentBank]->numControls; i++) {
//             uint8_t value = banks[currentBank]->values[i];

//             // Here you would send the value to the hardware, e.g., via MIDI CC messages
//             device->sendCC(banks[currentBank]->ccStartFrom + i, value, 0); // assuming channel 0 for now
//         }
//     }
//     int getBank() const { return currentBank; }

//     APC40Bank* getCurrentBank() const { return banks[currentBank]; }
//     void addBank(const char *label) {
//         if (numBanks < maxBanks) {
//             banks[numBanks++] = new APC40Bank(label, numControls);
//         }
//     }

//     void handleControlChange(uint8_t cc, uint8_t value, uint8_t channel) {
//         this->banks[currentBank]->updateValue(cc-ccStartFrom, value);
//     }

//     void handleNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
//         // Here you would handle note on events, e.g., for bank buttons
//         for (int i = 0; i < numBanks; i++) {
//             if (bank_buttons[i] && bank_buttons[i] == note) {
//                 setBank(i);
//             }
//         }
//     }

//     // Emit banked changes to the host app
//     std::function<void(int trackIndex, int parameterIndex, uint8_t value)> onTrackChange;
//     std::function<void(int deviceIndex, int parameterIndex, uint8_t value)> onDeviceChange;

// private:
//     APC40Device *device = nullptr;
//     int currentBank = 0;
//     char label[40] = "Unknown Banks";
    
//     static const uint8_t maxBanks = 16;
//     uint8_t numBanks = 0;
//     APC40Bank* banks[maxBanks] = { nullptr };
//     uint8_t ccStartFrom = 0;
//     uint8_t numControls = 8;

//     uint8_t bank_buttons[maxBanks] = { 0 };
// };
