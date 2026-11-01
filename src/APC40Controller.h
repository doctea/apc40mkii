#pragma once
#include "APC40Device.h"
#include "APC40Feedback.h"
#include "APCControl.h"

#include "APC40Constants.h"

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

enum class APC40ControlKind : uint8_t {
    Unknown, TrackKnob, DeviceKnob, TrackFader, MasterFader, Crossfader,
    CueEncoder, Button, Pad
};

struct APC40ControlId {
    APC40ControlKind kind = APC40ControlKind::Unknown;
    uint8_t bank = 0;
    uint8_t index = 0;
};

enum class APC40InputPhase : uint8_t { Change, Press, Release };

struct APC40ControlEvent {
    APC40ControlId control;
    APC40InputPhase phase;
    uint8_t value;
};

#ifdef ENABLE_PARAMETERS_MIDI_LEARN
    enum class MIDILearnState {
        Pending,        // host is waiting for MIDI learn to be activated on device
        Active,         // host is waiting and MIDI learn is currently active on the device
        Inactive        // host is not waiting and MIDI learn is not active on the device
    };
#endif

class APC40Controller {
public:

    APC40Device device;
    APC40Feedback feedback;

    GenericList<APC40BankSet*> bank_sets;
    GenericList<APCControl*> controls;

    explicit APC40Controller(
        APC40Transport& transport,
        APC40MKII::sysex::APPLICATION_MODES mode = APC40MKII::sysex::MODE_1
    );
    bool init();
    void loop();

    void setup_banks();

    void redraw(APCControl* ctrl);

    APCControl* find_control(uint8_t note, uint8_t channel, APCControlType type);

    APCControl* bind_cc(uint8_t cc, uint8_t channel);
    APCControl* bind_note(uint8_t note, uint8_t channel);

    std::function<void()> onPlayPressed;
    std::function<void()> onRecordPressed;
    std::function<void()> onSessionPressed;

    void setPlayState(bool active);
    void setRecordState(bool active);
    void setSessionState(bool active);

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
    bool getKnobState(APC40ControlId control, APC40VirtualEncoderState& state) const;
    bool setKnobState(APC40ControlId control, const APC40VirtualEncoderState& state);

    static const char* getTrackKnobBankName(APC40TrackKnobBank bank);
    static const char* getTrackKnobGroupName(APC40TrackKnobBank bank);
    static const char* getTrackKnobName(APC40TrackKnobBank bank, uint8_t track);
    static const char* getTrackFaderName(uint8_t track);

    #ifdef ENABLE_PARAMETERS_MIDI_LEARN
        bool midiLearnHostState = false;
        bool midiLearnDeviceState = false;
        void setMIDILearnDeviceState(bool state);
        void setMIDILearnHostState(bool state);
        void updateMidiLearnState();
        MIDILearnState getMidiLearnState() const;
        std::function<bool(const APC40ControlEvent&)> onLearnCandidate;
    #endif

private:
    struct TransportButtonState {
        APCControl* control = nullptr;
        bool initialState = false;
        bool pressed = false;
        bool hasSentState = false;
        bool sentState = false;
        uint8_t sentVelocity = 0;
    };

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
    TransportButtonState transportButtons[3];
    bool sessionState = false;
    #ifdef ENABLE_PARAMETERS_MIDI_LEARN
        uint32_t midiLearnBlinkStart = 0;
        void proposeLearnCandidate(const APC40ControlEvent& event);
    #endif
    bool handleTransportButton(uint8_t note, uint8_t channel, uint8_t velocity);
    void setTransportButtonState(uint8_t index, bool active);
    int16_t applyRelativeAcceleration(uint8_t value, RelativeEncoderState& state);
    void synchronize();
};
