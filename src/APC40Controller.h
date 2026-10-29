#pragma once
#include "APC40Device.h"
#include "APC40Banks.h"
#include "APC40BankSet.h"
#include "APC40Feedback.h"

#include "constants.h"

class APC40Controller {
public:
    APC40Controller();   // add this
    bool init();
    void poll(); // currently unused, but useful later

    void setup_banks();

    APC40Device device;
    APC40BankSet *bank_sets[2];

    APC40Feedback feedback;
};
