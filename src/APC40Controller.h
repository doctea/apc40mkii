#pragma once
#include "APC40Device.h"
// #include "APC40BankSet.h"
#include "APC40Feedback.h"
#include "APCControl.h"

#include "constants.h"

#include "GenericList.h"
#include <cstdint>
#include <functional>

class APC40BankSet;

enum class APC40TrackKnobBank : uint8_t {
    Pan,
    Sends,
    User
};

struct APC40EncoderAcceleration {
    uint16_t thresholdMs = 0;
    uint8_t multiplier = 1;
};

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

    std::function<void(uint8_t, uint8_t)> onTrackFaderChanged;
    std::function<void(uint8_t)> onMasterFaderChanged;
    std::function<void(uint8_t)> onCrossfaderChanged;
    std::function<void(int16_t)> onTempoEncoderDelta;
    std::function<void(int16_t)> onCueEncoderDelta;
    std::function<void(APC40TrackKnobBank, uint8_t, uint8_t)> onTrackKnobChanged;

    void setMillisProvider(std::function<uint32_t()> provider);
    void setTempoEncoderAcceleration(APC40EncoderAcceleration acceleration);
    void setCueEncoderAcceleration(APC40EncoderAcceleration acceleration);

    bool getTrackKnobState(APC40TrackKnobBank bank, uint8_t track,
        APC40VirtualEncoderState& state) const;
    bool setTrackKnobState(APC40TrackKnobBank bank, uint8_t track,
        const APC40VirtualEncoderState& state);

    APC40Device device;
    APC40Feedback feedback;
    
    GenericList<APC40BankSet*> bank_sets;
    
    GenericList<APCControl*> controls;

private:
    struct RelativeEncoderState {
        APC40EncoderAcceleration acceleration;
        uint32_t lastInputMillis = 0;
        bool hasPreviousInput = false;
    };

    APC40MKII::sysex::APPLICATION_MODES mode;
    bool initialized = false;
    bool synchronized = false;
    std::function<uint32_t()> millisProvider;
    RelativeEncoderState tempoEncoderState;
    RelativeEncoderState cueEncoderState;
    int16_t applyRelativeAcceleration(uint8_t value, RelativeEncoderState& state);
    void synchronize();
};
