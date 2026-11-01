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
        if (handleTransportButton(note, channel, vel))
            return;

        for (auto *bs : bank_sets) {
            bs->handleButton(note, vel, channel);
        }

        for (auto* ctrl : controls) {
            if (ctrl == nullptr) continue;
            if (
                ctrl->type == APCControlType::Button
                && ctrl->midiNote == note
                && ctrl->midiChannel == channel
            ) {
                ctrl->applyAPCEvent(vel);
                redraw(ctrl);
            }
        }
    };

    device.onNoteOff = [this](uint8_t note, uint8_t channel, uint8_t) {
        handleTransportButton(note, channel, 0);
    };

    device.handleControlChange = [&](uint8_t cc, uint8_t channel, uint8_t val) {
        bool handled = false;
        #ifdef ENABLE_PARAMETERS_MIDI_LEARN
            APC40ControlId candidate;
        #endif

        if (cc == APC40MKII::tracks::CC_FADER && channel < 8) {
            if (onTrackFaderChanged) {
                onTrackFaderChanged(channel, val);
                handled = true;
            }
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                candidate = {APC40ControlKind::TrackFader, 0, channel};
            #endif
        } else if (cc == APC40MKII::tracks::CC_FADER_MASTER && channel == 0) {
            if (onMasterFaderChanged) {
                onMasterFaderChanged(val);
                handled = true;
            }
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                candidate = {APC40ControlKind::MasterFader, 0, 0};
            #endif
        } else if (cc == APC40MKII::CC_CROSSFADER && channel == 0) {
            if (onCrossfaderChanged) {
                onCrossfaderChanged(val);
                handled = true;
            }
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                candidate = {APC40ControlKind::Crossfader, 0, 0};
            #endif
        } else if (cc == APC40MKII::tempo::CC_TEMPO && channel == 0 && onTempoEncoderDelta) {
            onTempoEncoderDelta(applyRelativeAcceleration(val, tempoEncoderState));
            handled = true;
        } else if (cc == APC40MKII::tracks::CC_CUE_LEVEL && channel == 0) {
            if (onCueEncoderDelta) {
                onCueEncoderDelta(applyRelativeAcceleration(val, cueEncoderState));
                handled = true;
            }
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                if (val != 0)
                    candidate = {APC40ControlKind::CueEncoder, 0, 0};
            #endif
        }

        for (auto* bs : bank_sets) {
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                const uint8_t inputBank = static_cast<uint8_t>(bs->getCurrentBank());
            #endif
            if (!bs->handleControlChange(cc, val, channel))
                continue;
            handled = true;
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                if (bs == bank_sets[0]) {
                    candidate = {APC40ControlKind::TrackKnob,
                        inputBank,
                        static_cast<uint8_t>(cc - APC40MKII::tracks::CC_KNOB_1)};
                } else if (bank_sets.size() > 1 && bs == bank_sets[1]) {
                    candidate = {APC40ControlKind::DeviceKnob,
                        inputBank,
                        static_cast<uint8_t>(cc - APC40MKII::device::CC_KNOB_1)};
                }
            #endif
        }
        
        for (auto* ctrl : controls) {
            if (ctrl == nullptr) continue;
            if (ctrl->type==APCControlType::Encoder && ctrl->midiCC == cc && ctrl->midiChannel == channel) {
                ctrl->applyAPCEvent(val);
                handled = true;
            }
        }
        #ifdef ENABLE_PARAMETERS_MIDI_LEARN
            proposeLearnCandidate({candidate, APC40InputPhase::Change, val});
        #endif
        return handled;
    };

    transportButtons[0].control = new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_PLAY, 0, "Play", true);
    transportButtons[1].control = new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_RECORD, 0, "Record", true);
    transportButtons[2].control = new APCControl(APCControlType::Button, APC40MKII::transport::NOTE_BUTTON_SESSION, 0, "Session", true);
    for (auto& button : transportButtons) {
        button.control->setAppValue(button.initialState ? 1.0f : 0.0f);
        controls.add(button.control);
    }

    setup_banks();

    device.onConnectionChanged = [this](bool connected) {
        synchronized = false;
        for (auto& button : transportButtons) {
            button.pressed = false;
            button.hasSentState = false;
        }
        #ifdef ENABLE_PARAMETERS_MIDI_LEARN
            if (!connected)
                setMIDILearnDeviceState(false);
        #endif
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
    #ifdef ENABLE_PARAMETERS_MIDI_LEARN
        updateMidiLearnState();
    #endif
    for (auto& button : transportButtons) {
        if (button.control)
            redraw(button.control);
    }
}

void APC40Controller::setPlayState(bool active) {
    setTransportButtonState(0, active);
}

void APC40Controller::setRecordState(bool active) {
    setTransportButtonState(1, active);
}

void APC40Controller::setSessionState(bool active) {
    sessionState = active;
    #ifdef ENABLE_PARAMETERS_MIDI_LEARN
        if (midiLearnHostState)
            return;
    #endif
    setTransportButtonState(2, active);
}

void APC40Controller::setTransportButtonState(uint8_t index, bool active) {
    auto& button = transportButtons[index];
    if (!button.control) {
        button.initialState = active;
        return;
    }
    if ((button.control->value > 0.0f) != active)
        button.control->setAppValue(active ? 1.0f : 0.0f);
    redraw(button.control);
}

bool APC40Controller::handleTransportButton(uint8_t note, uint8_t channel, uint8_t velocity) {
    for (uint8_t index = 0; index < 3; ++index) {
        auto& button = transportButtons[index];
        if (!button.control || button.control->midiNote != note || button.control->midiChannel != channel)
            continue;
        const bool pressed = velocity > 0;
        if (button.pressed == pressed)
            return true;
        button.pressed = pressed;
        if (pressed) {
            auto& callback = index == 0 ? onPlayPressed : index == 1 ? onRecordPressed : onSessionPressed;
            if (callback)
                callback();
            else if (button.control->onAPCValueChanged)
                button.control->onAPCValueChanged(velocity);
            #ifdef ENABLE_PARAMETERS_MIDI_LEARN
                else if (index == 2)
                    setMIDILearnDeviceState(!midiLearnDeviceState);
            #endif
            redraw(button.control);
        }
        return true;
    }
    return false;
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
    // @@TODO: we should ensure that this only sends updates if necessary (value has changed, or device has reconnected)
    if (ctrl->type==APCControlType::Button) {
        // Update the LED state for the button
        if (ctrl->hasLED) {
            for (auto& button : transportButtons) {
                if (button.control != ctrl)
                    continue;
                const bool active = ctrl->value > 0.0f;
                const uint8_t velocity = active ? ctrl->ledOn : ctrl->ledOff;
                if (button.hasSentState && button.sentState == active && button.sentVelocity == velocity)
                    return;
                const bool sent = active
                    ? device.sendNoteOn(ctrl->midiNote, velocity, ctrl->midiChannel)
                    : device.sendNoteOff(ctrl->midiNote, velocity, ctrl->midiChannel);
                if (sent) {
                    button.hasSentState = true;
                    button.sentState = active;
                    button.sentVelocity = velocity;
                }
                return;
            }
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

bool APC40Controller::getKnobState(APC40ControlId control, APC40VirtualEncoderState& state) const {
    const int bankSetIndex = control.kind == APC40ControlKind::TrackKnob ? 0 :
        control.kind == APC40ControlKind::DeviceKnob ? 1 : -1;
    if (bankSetIndex < 0 || bankSetIndex >= static_cast<int>(bank_sets.size()))
        return false;
    return bank_sets[bankSetIndex]->getEncoderState(control.index, control.bank, state);
}

bool APC40Controller::setKnobState(APC40ControlId control, const APC40VirtualEncoderState& state) {
    const int bankSetIndex = control.kind == APC40ControlKind::TrackKnob ? 0 :
        control.kind == APC40ControlKind::DeviceKnob ? 1 : -1;
    if (bankSetIndex < 0 || bankSetIndex >= static_cast<int>(bank_sets.size()))
        return false;
    return bank_sets[bankSetIndex]->setEncoderState(control.index, control.bank, state);
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

#ifdef ENABLE_PARAMETERS_MIDI_LEARN
    void APC40Controller::setMIDILearnHostState(bool state) {
        if (midiLearnHostState == state)
            return;
        midiLearnHostState = state;
        if (!state)
            midiLearnDeviceState = false;
        midiLearnBlinkStart = millisProvider ? millisProvider() : 0;
        updateMidiLearnState();
    }
    void APC40Controller::setMIDILearnDeviceState(bool state) {
        state = state && midiLearnHostState && device.isConnected();
        if (midiLearnDeviceState == state)
            return;
        midiLearnDeviceState = state;
        midiLearnBlinkStart = millisProvider ? millisProvider() : 0;
        updateMidiLearnState();
    }
    void APC40Controller::updateMidiLearnState() {
        bool active = sessionState;
        if (midiLearnHostState)
            active = !midiLearnDeviceState || !millisProvider ||
                ((static_cast<uint32_t>(millisProvider() - midiLearnBlinkStart) / 250) % 2 == 0);
        setTransportButtonState(2, active);
    }
    MIDILearnState APC40Controller::getMidiLearnState() const {
        if (midiLearnHostState && midiLearnDeviceState) {
            return MIDILearnState::Active;
        } else if (midiLearnHostState) {
            return MIDILearnState::Pending;
        } else {
            return MIDILearnState::Inactive;
        }
    }
    void APC40Controller::proposeLearnCandidate(const APC40ControlEvent& event) {
        if (getMidiLearnState() != MIDILearnState::Active || !onLearnCandidate ||
                event.control.kind == APC40ControlKind::Unknown ||
                event.phase == APC40InputPhase::Release)
            return;
        if (onLearnCandidate(event))
            setMIDILearnDeviceState(false);
    }
#endif