#include "APC40Controller.h"
#include "APC40BankSet.h"

#include "APC40Constants.h"

APC40Controller::APC40Controller(APC40Transport& transport,
        APC40MKII::sysex::APPLICATION_MODES mode)
    : device(transport),
            feedback(device),
            mode(mode)
{
}

bool APC40Controller::init() {
    if (initialized) {
        if (!device.open())
            return false;
        synchronize();
        return device.isConnected();
    }

    device.onNoteOn = [&](uint8_t note, uint8_t channel, uint8_t vel) {
        for (auto *bs : bank_sets) {
            bs->handleButton(note, vel, channel);
        }

        for (auto* ctrl : controls) {
            if (ctrl == nullptr) continue;
            if (ctrl->type == APCControlType::Button && ctrl->midiNote == note && ctrl->midiChannel == channel) {
                ctrl->applyAPCEvent(vel);
                redraw(ctrl);
            }
        }
    };

    device.handleControlChange = [&](uint8_t cc, uint8_t channel, uint8_t val) {
        bool handled = true;
        if (cc == APC40MKII::tracks::CC_FADER && channel < 8 && onTrackFaderChanged) {
            onTrackFaderChanged(channel, val);
        } else if (cc == APC40MKII::tracks::CC_FADER_MASTER && channel == 0 && onMasterFaderChanged) {
            onMasterFaderChanged(val);
        } else if (cc == APC40MKII::CC_CROSSFADER && channel == 0 && onCrossfaderChanged) {
            onCrossfaderChanged(val);
        } else if (cc == APC40MKII::tempo::CC_TEMPO && channel == 0 && onTempoEncoderDelta) {
            onTempoEncoderDelta(applyRelativeAcceleration(val, tempoEncoderState));
        } else if (cc == APC40MKII::tracks::CC_CUE_LEVEL && channel == 0 && onCueEncoderDelta) {
            onCueEncoderDelta(applyRelativeAcceleration(val, cueEncoderState));
        } else {
            handled = false;
        }

        for (auto* bs : bank_sets) {
            handled = bs->handleControlChange(cc, val, channel) || handled;
        }
        
        for (auto* ctrl : controls) {
            if (ctrl == nullptr) continue;
            if (ctrl->type==APCControlType::Encoder && ctrl->midiCC == cc && ctrl->midiChannel == channel) {
                ctrl->applyAPCEvent(val);
                handled = true;
            }
        }
        return handled;
    };

    controls.add(new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_PLAY, 0, "Play", true));
    controls.add(new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_RECORD, 0, "Record", true));
    controls.add(new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_SESSION, 0, "Session", true));

    setup_banks();

    device.onConnectionChanged = [this](bool connected) {
        synchronized = false;
        if (connected)
            synchronize();
    };
    initialized = true;
    if (!device.open())
        return false;
    synchronize();
    return device.isConnected();
}

void APC40Controller::synchronize() {
    if (!initialized || synchronized || !device.isConnected())
        return;

    synchronized = true;
    const uint8_t introduction[] = {static_cast<uint8_t>(mode), 0x09, 0x07, 0x01};
    device.send_sysex_command(APC40MKII::sysex::MESSAGE_TYPE_INTRODUCTION_IDENTIFIER,
        sizeof(introduction), introduction);
    for (auto* banks : bank_sets)
        banks->setBank(banks->getCurrentBank());
    for (auto* control : controls)
        redraw(control);
}

void APC40Controller::loop() {
    device.loop();
}

void APC40Controller::setMillisProvider(std::function<uint32_t()> provider) {
    millisProvider = provider;
    tempoEncoderState.hasPreviousInput = false;
    cueEncoderState.hasPreviousInput = false;
}

void APC40Controller::setTempoEncoderAcceleration(APC40EncoderAcceleration acceleration) {
    tempoEncoderState.acceleration = acceleration;
    tempoEncoderState.hasPreviousInput = false;
}

void APC40Controller::setCueEncoderAcceleration(APC40EncoderAcceleration acceleration) {
    cueEncoderState.acceleration = acceleration;
    cueEncoderState.hasPreviousInput = false;
}

int16_t APC40Controller::applyRelativeAcceleration(uint8_t value, RelativeEncoderState& state) {
    const int16_t delta = static_cast<int16_t>(value < 64 ? value : static_cast<int>(value) - 128);
    if (delta == 0 || !millisProvider)
        return delta;

    const uint32_t now = millisProvider();
    const bool accelerate = state.hasPreviousInput && state.acceleration.thresholdMs > 0 &&
        state.acceleration.multiplier > 1 &&
        static_cast<uint32_t>(now - state.lastInputMillis) <= state.acceleration.thresholdMs;
    state.lastInputMillis = now;
    state.hasPreviousInput = true;
    return static_cast<int16_t>(delta * (accelerate ? state.acceleration.multiplier : 1));
}

void APC40Controller::redraw(APCControl* ctrl) {
    // Implement the logic to update the APC40 MkII hardware based on the control's state
    if (ctrl->type==APCControlType::Button) {
        // Update the LED state for the button
        if (ctrl->hasLED) {
            if (ctrl->value > 0.0f)
                device.sendNoteOn(ctrl->midiNote, ctrl->ledOn, ctrl->midiChannel);
            else
                device.sendNoteOff(ctrl->midiNote, ctrl->ledOff, ctrl->midiChannel);
        }
    } else if (ctrl->type==APCControlType::Encoder) {
        // Update the LED state for the encoder if needed
        if (ctrl->hasLED) {
            if (ctrl->value > 0.0f)
                device.sendCC(ctrl->midiCC, ctrl->ledOn, ctrl->midiChannel);
            else
                device.sendCC(ctrl->midiCC, ctrl->ledOff, ctrl->midiChannel);
        }
    }
}


void APC40Controller::setup_banks() {
    if (!bank_sets.empty())
        return;

    // track knob buttons and labels
    uint8_t track_buttons[] = {
        APC40MKII::tracks::NOTE_BUTTON_PAN,
        APC40MKII::tracks::NOTE_BUTTON_SENDS,
        APC40MKII::tracks::NOTE_BUTTON_USER 
    };
    static const char* track_labels[] = {
        getTrackKnobBankName(APC40TrackKnobBank::Pan),
        getTrackKnobBankName(APC40TrackKnobBank::Sends),
        getTrackKnobBankName(APC40TrackKnobBank::User)
    };
    auto* trackKnobs = new APC40BankSet(
        this, "Track Knobs", 3,
        track_buttons,
        track_labels,
        APC40MKII::tracks::CC_KNOB_1, 8
    );
    trackKnobs->onBankedChange = [this](int track, int bank, uint8_t value) {
        if (onTrackKnobChanged)
            onTrackKnobChanged(static_cast<APC40TrackKnobBank>(bank),
                static_cast<uint8_t>(track), value);
    };
    bank_sets.add(trackKnobs);

    // device knobs buttons and labels
    uint8_t device_buttons[] = { 
        APC40MKII::device::NOTE_BUTTON_1,
        APC40MKII::device::NOTE_BUTTON_2,
        APC40MKII::device::NOTE_BUTTON_3,
        APC40MKII::device::NOTE_BUTTON_4,
        APC40MKII::device::NOTE_BUTTON_5,
        APC40MKII::device::NOTE_BUTTON_6,
        APC40MKII::device::NOTE_BUTTON_7,
        APC40MKII::device::NOTE_BUTTON_8
    };
    static const char* device_labels[] = {
        "Device 1",
        "Device 2",
        "Device 3",
        "Device 4",
        "Device 5",
        "Device 6",
        "Device 7",
        "Device 8"
    };
    bank_sets.add(new APC40BankSet(
        this, "Device Knobs", 8,
        device_buttons,
        device_labels,
        APC40MKII::device::CC_KNOB_1, 8
    ));

}

bool APC40Controller::getTrackKnobState(APC40TrackKnobBank bank, uint8_t track,
        APC40VirtualEncoderState& state) const {
    const uint8_t bankIndex = static_cast<uint8_t>(bank);
    if (bankIndex > static_cast<uint8_t>(APC40TrackKnobBank::User) || bank_sets.empty())
        return false;
    return bank_sets[0]->getEncoderState(track, bankIndex, state);
}

bool APC40Controller::setTrackKnobState(APC40TrackKnobBank bank, uint8_t track,
        const APC40VirtualEncoderState& state) {
    const uint8_t bankIndex = static_cast<uint8_t>(bank);
    if (bankIndex > static_cast<uint8_t>(APC40TrackKnobBank::User) || bank_sets.empty())
        return false;
    return bank_sets[0]->setEncoderState(track, bankIndex, state);
}

const char* APC40Controller::getTrackKnobBankName(APC40TrackKnobBank bank) {
    static const char* const names[] = {"Pan", "Sends", "User"};
    const uint8_t bankIndex = static_cast<uint8_t>(bank);
    return bankIndex < 3 ? names[bankIndex] : nullptr;
}

const char* APC40Controller::getTrackKnobGroupName(APC40TrackKnobBank bank) {
    static const char* const names[] = {"APC40 Pan", "APC40 Sends", "APC40 User"};
    const uint8_t bankIndex = static_cast<uint8_t>(bank);
    return bankIndex < 3 ? names[bankIndex] : nullptr;
}

const char* APC40Controller::getTrackKnobName(APC40TrackKnobBank bank, uint8_t track) {
    static const char* const names[3][8] = {
        {"PanKnob1", "PanKnob2", "PanKnob3", "PanKnob4", "PanKnob5", "PanKnob6", "PanKnob7", "PanKnob8"},
        {"SendsKnob1", "SendsKnob2", "SendsKnob3", "SendsKnob4", "SendsKnob5", "SendsKnob6", "SendsKnob7", "SendsKnob8"},
        {"UserKnob1", "UserKnob2", "UserKnob3", "UserKnob4", "UserKnob5", "UserKnob6", "UserKnob7", "UserKnob8"}
    };
    const uint8_t bankIndex = static_cast<uint8_t>(bank);
    return bankIndex < 3 && track < 8 ? names[bankIndex][track] : nullptr;
}

const char* APC40Controller::getTrackFaderName(uint8_t track) {
    static const char* const names[] = {
        "TrackFader1", "TrackFader2", "TrackFader3", "TrackFader4",
        "TrackFader5", "TrackFader6", "TrackFader7", "TrackFader8"
    };
    return track < 8 ? names[track] : nullptr;
}

APCControl* APC40Controller::bind_cc(uint8_t cc, uint8_t channel) {
    return find_control(cc, channel, APCControlType::Encoder);
}

APCControl* APC40Controller::bind_note(uint8_t note, uint8_t channel) {
    return find_control(note, channel, APCControlType::Button);
}

APCControl* APC40Controller::find_control(uint8_t note, uint8_t channel, APCControlType type) {
    for (auto* ctrl : controls) {
        if (
            ctrl->midiChannel == channel && 
            ctrl->type == type && (
                (ctrl->type == APCControlType::Button && ctrl->midiNote == note) 
                || (ctrl->type == APCControlType::Encoder && ctrl->midiCC == note))
            ) {
            return ctrl;
        }
    }
    return nullptr;
}

