#pragma once
#include "APC40Device.h"
// #include "APC40BankSet.h"
#include "APC40Feedback.h"
#include "APCControl.h"

#include "constants.h"

#include "GenericList.h"

class APC40BankSet;

class APC40Controller {
public:
    explicit APC40Controller(APC40Transport& transport,
        APC40MKII::sysex::APPLICATION_MODES mode = APC40MKII::sysex::MODE_1);
    bool init();
    void loop();

    void setup_banks();

    void redraw(APCControl* ctrl);

    APCControl* find_control(uint8_t note, uint8_t channel, APCControlType type);

    APCControl* bind_cc(uint8_t cc, uint8_t channel);
    APCControl* bind_note(uint8_t note, uint8_t channel);

    APC40Device device;
    APC40Feedback feedback;
    
    GenericList<APC40BankSet*> bank_sets;
    
    GenericList<APCControl*> controls;

private:
    APC40MKII::sysex::APPLICATION_MODES mode;
    bool initialized = false;
    bool synchronized = false;
    void synchronize();
};
