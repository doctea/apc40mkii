#pragma once

#if defined(APC40_ENABLE_PARAMETERS) && APC40_ENABLE_PARAMETERS

#include "APC40Controller.h"
#include "parameter_inputs/MIDIParameterInput.h"

#ifdef ENABLE_APC40_EXTENDED_INPUTS
    #ifndef ENABLE_APC40_DEVICE_KNOBS
        #define ENABLE_APC40_DEVICE_KNOBS
    #endif
    #ifndef ENABLE_APC40_CLIP_PADS
        #define ENABLE_APC40_CLIP_PADS
    #endif
    #ifndef ENABLE_APC40_TRACK_BUTTONS
        #define ENABLE_APC40_TRACK_BUTTONS
    #endif
    #ifndef ENABLE_APC40_GENERAL_BUTTONS
        #define ENABLE_APC40_GENERAL_BUTTONS
    #endif
#endif

#if defined(ENABLE_APC40_CLIP_PADS) || defined(ENABLE_APC40_TRACK_BUTTONS) || \
        defined(ENABLE_APC40_GENERAL_BUTTONS)
    #define APC40_HAS_NOTE_INPUTS
#endif

class APC40LogicalKnobParameterInput : public MIDIParameterInputBase {
    private:
        APC40Controller &controller;
        const APC40ControlId control;

    public:
        APC40LogicalKnobParameterInput(char *name, const char *group_name,
                APC40Controller &controller, APC40TrackKnobBank bank, uint8_t track)
            : APC40LogicalKnobParameterInput(name, group_name, controller,
                APC40ControlId{APC40ControlKind::TrackKnob, static_cast<uint8_t>(bank), track}) {}

        APC40LogicalKnobParameterInput(char *name, const char *group_name,
                APC40Controller &controller, APC40ControlId control)
            : MIDIParameterInputBase(name, group_name), controller(controller), control(control) {
            this->loop();
        }

        APC40LogicalKnobParameterInput(APC40Controller &controller, APC40TrackKnobBank bank, uint8_t track)
            : APC40LogicalKnobParameterInput(
                const_cast<char *>(APC40Controller::getTrackKnobName(bank, track)
                    ? APC40Controller::getTrackKnobName(bank, track) : "UnknownKnob"),
                APC40Controller::getTrackKnobGroupName(bank)
                    ? APC40Controller::getTrackKnobGroupName(bank) : "APC40",
                controller, bank, track) {}

        APC40LogicalKnobParameterInput(APC40Controller &controller, uint8_t deviceBank, uint8_t knob)
            : APC40LogicalKnobParameterInput(
                const_cast<char *>(getDeviceKnobName(deviceBank, knob)
                    ? getDeviceKnobName(deviceBank, knob) : "UnknownDevice"),
                getDeviceKnobGroupName(deviceBank)
                    ? getDeviceKnobGroupName(deviceBank) : "APC40",
                controller, APC40ControlId{APC40ControlKind::DeviceKnob, deviceBank, knob}) {}

        static const char *getDeviceKnobGroupName(uint8_t bank) {
            static const char *const names[] = {
                "APC40 Device1", "APC40 Device2", "APC40 Device3", "APC40 Device4",
                "APC40 Device5", "APC40 Device6", "APC40 Device7", "APC40 Device8"
            };
            return bank < 8 ? names[bank] : nullptr;
        }

        static const char *getDeviceKnobName(uint8_t bank, uint8_t knob) {
            static const char *const names[8][8] = {
                {"Device1Knob1", "Device1Knob2", "Device1Knob3", "Device1Knob4", "Device1Knob5", "Device1Knob6", "Device1Knob7", "Device1Knob8"},
                {"Device2Knob1", "Device2Knob2", "Device2Knob3", "Device2Knob4", "Device2Knob5", "Device2Knob6", "Device2Knob7", "Device2Knob8"},
                {"Device3Knob1", "Device3Knob2", "Device3Knob3", "Device3Knob4", "Device3Knob5", "Device3Knob6", "Device3Knob7", "Device3Knob8"},
                {"Device4Knob1", "Device4Knob2", "Device4Knob3", "Device4Knob4", "Device4Knob5", "Device4Knob6", "Device4Knob7", "Device4Knob8"},
                {"Device5Knob1", "Device5Knob2", "Device5Knob3", "Device5Knob4", "Device5Knob5", "Device5Knob6", "Device5Knob7", "Device5Knob8"},
                {"Device6Knob1", "Device6Knob2", "Device6Knob3", "Device6Knob4", "Device6Knob5", "Device6Knob6", "Device6Knob7", "Device6Knob8"},
                {"Device7Knob1", "Device7Knob2", "Device7Knob3", "Device7Knob4", "Device7Knob5", "Device7Knob6", "Device7Knob7", "Device7Knob8"},
                {"Device8Knob1", "Device8Knob2", "Device8Knob3", "Device8Knob4", "Device8Knob5", "Device8Knob6", "Device8Knob7", "Device8Knob8"}
            };
            return bank < 8 && knob < 8 ? names[bank][knob] : nullptr;
        }

        APC40TrackKnobBank get_bank() const { return static_cast<APC40TrackKnobBank>(this->control.bank); }
        uint8_t get_track() const { return this->control.index; }
        APC40ControlId get_control_id() const { return this->control; }

        uint8_t get_value() const {
            APC40VirtualEncoderState state;
            return this->controller.getKnobState(this->control, state) ? state.value : 0;
        }

        bool set_value(uint8_t value) {
            APC40VirtualEncoderState state;
            if (!this->controller.getKnobState(this->control, state)) {
                return false;
            }
            value = value > 127 ? 127 : value;
            const APC40EncoderPolarity polarity = this->input_type == BIPOLAR
                ? APC40EncoderPolarity::Bipolar : APC40EncoderPolarity::Unipolar;
            if (state.value == value && state.polarity == polarity &&
                    state.ringMode == APC40EncoderRingMode::Automatic) {
                return true;
            }
            state.value = value;
            state.polarity = polarity;
            state.ringMode = APC40EncoderRingMode::Automatic;
            return this->controller.setKnobState(this->control, state);
        }

        virtual void loop() override {
            this->set_value(this->get_value());
        }

        virtual const char *getInputValue() override {
            static char formatted_value[20];
            snprintf(formatted_value, sizeof(formatted_value), "[%3u]",
                static_cast<unsigned>(this->get_value()));
            return formatted_value;
        }

        virtual const char *getOutputValue() override {
            static char formatted_value[20];
            const float value = this->input_type == BIPOLAR
                ? this->get_normal_value_bipolar() : this->get_normal_value_unipolar();
            snprintf(formatted_value, sizeof(formatted_value), "[%-4i%%]",
                static_cast<int>(value * 100.0f));
            return formatted_value;
        }

        virtual bool supports_bipolar_input() override {
            return true;
        }

        virtual float get_normal_value_unipolar() override {
            return static_cast<float>(this->get_value()) / 127.0f;
        }

        virtual float get_normal_value_bipolar() override {
            const float bipolar = (static_cast<float>(this->get_value()) - 64.0f) / 63.0f;
            return constrain(bipolar, -1.0f, 1.0f);
        }

        virtual const char *getInputInfo() override {
            static char input_info[24];
            if (this->control.kind == APC40ControlKind::DeviceKnob) {
                snprintf(input_info, sizeof(input_info), "Device %u knob %u",
                    static_cast<unsigned>(this->control.bank + 1),
                    static_cast<unsigned>(this->control.index + 1));
            } else {
                const char *bank_name = APC40Controller::getTrackKnobBankName(this->get_bank());
                snprintf(input_info, sizeof(input_info), "%s knob %u", bank_name ? bank_name : "Unknown",
                    static_cast<unsigned>(this->control.index + 1));
            }
            return input_info;
        }
};

enum class APC40NoteParameterInputMode : uint8_t { Momentary, Toggle };

class APC40NoteParameterInput : public MIDIParameterInputBase {
    private:
        const APC40ControlId control;
        APC40NoteParameterInputMode mode = APC40NoteParameterInputMode::Momentary;
        bool pressed = false;
        bool active = false;

    public:
        APC40NoteParameterInput(char *name, const char *group_name, APC40ControlId control)
            : MIDIParameterInputBase(name, group_name), control(control) {
            this->input_type = UNIPOLAR;
        }

        APC40ControlId get_control_id() const { return this->control; }
        APC40NoteParameterInputMode get_mode() const { return this->mode; }
        bool is_active() const { return this->active; }

        void set_mode(APC40NoteParameterInputMode mode) {
            if (this->mode == mode)
                return;
            this->mode = mode;
            if (mode == APC40NoteParameterInputMode::Momentary)
                this->active = this->pressed;
        }

        void apply(APC40InputPhase phase) {
            if (phase == APC40InputPhase::Press) {
                if (this->pressed)
                    return;
                this->pressed = true;
                if (this->mode == APC40NoteParameterInputMode::Toggle)
                    this->active = !this->active;
                else
                    this->active = true;
            } else if (phase == APC40InputPhase::Release) {
                this->pressed = false;
                if (this->mode == APC40NoteParameterInputMode::Momentary)
                    this->active = false;
            }
        }

        virtual bool supports_bipolar_input() override { return false; }
        virtual float get_normal_value_unipolar() override { return this->active ? 1.0f : 0.0f; }
        virtual float get_normal_value_bipolar() override { return this->active ? 1.0f : -1.0f; }

        virtual const char *getInputValue() override {
            return this->active ? "On" : "Off";
        }

        virtual const char *getOutputValue() override {
            return this->active ? "[100%]" : "[0%]";
        }

        virtual const char *getInputInfo() override {
            static char info[32];
            snprintf(info, sizeof(info), "%s note %u ch %u",
                this->control.kind == APC40ControlKind::Pad ? "Pad" : "Button",
                static_cast<unsigned>(this->control.index),
                static_cast<unsigned>(this->control.bank + 1));
            return info;
        }
};

class APC40CueKnobParameterInput : public MIDICCParameterInput {
    public:
        APC40CueKnobParameterInput(char *name, const char *group_name)
            : MIDICCParameterInput(name, group_name, APC40MKII::tracks::CC_CUE_LEVEL,
                APC40MKII::tracks::CHAN_KNOBS) {}

        virtual bool responds_to_cc(byte, byte) override { return false; }
        virtual void receive_control_change(byte, byte, byte) override {}

        void apply_delta(int16_t delta) {
            const int value = static_cast<int>(this->currentValue) + delta;
            this->currentValue = static_cast<int8_t>(constrain(value, 0, 127));
        }

        virtual const char *getInputInfo() override {
            return "APC40 relative Cue";
        }
};

class APC40ParameterInputMap {
    private:
        static constexpr int deviceKnobOffset = 3 * 8;
#ifdef ENABLE_APC40_DEVICE_KNOBS
    static constexpr int deviceKnobCount = 8 * 8;
#else
    static constexpr int deviceKnobCount = 0;
#endif
    static constexpr int trackFaderOffset = deviceKnobOffset + deviceKnobCount;
        static constexpr int masterFaderOffset = trackFaderOffset + 8;
        static constexpr int crossfaderOffset = masterFaderOffset + 1;
        static constexpr int cueEncoderOffset = crossfaderOffset + 1;
        BaseParameterInput *inputs[cueEncoderOffset + 1] = {};
#ifdef APC40_HAS_NOTE_INPUTS
        static constexpr uint16_t noteInputCapacity = 128;
        struct NoteBinding {
            APC40ControlId control;
            APC40NoteParameterInput *input = nullptr;
        };
        NoteBinding noteInputs[noteInputCapacity] = {};
        uint16_t noteInputCount = 0;
    #endif

        static int slotFor(APC40ControlId control) {
            switch (control.kind) {
                case APC40ControlKind::TrackKnob:
                    return control.bank < 3 && control.index < 8 ? control.bank * 8 + control.index : -1;
#ifdef ENABLE_APC40_DEVICE_KNOBS
                case APC40ControlKind::DeviceKnob:
                    return control.bank < 8 && control.index < 8
                        ? deviceKnobOffset + control.bank * 8 + control.index : -1;
#endif
                case APC40ControlKind::TrackFader:
                    return control.bank == 0 && control.index < 8 ? trackFaderOffset + control.index : -1;
                case APC40ControlKind::MasterFader:
                    return control.bank == 0 && control.index == 0 ? masterFaderOffset : -1;
                case APC40ControlKind::Crossfader:
                    return control.bank == 0 && control.index == 0 ? crossfaderOffset : -1;
                case APC40ControlKind::CueEncoder:
                    return control.bank == 0 && control.index == 0 ? cueEncoderOffset : -1;
                default:
                    return -1;
            }
        }

    public:
        bool bind(APC40ControlId control, BaseParameterInput *input) {
            const int slot = slotFor(control);
            if (slot < 0)
                return false;
            inputs[slot] = input;
            return true;
        }

        bool bind_note(APC40ControlId control, APC40NoteParameterInput *input) {
    #ifdef APC40_HAS_NOTE_INPUTS
            if (!input ||
                    (control.kind != APC40ControlKind::Button && control.kind != APC40ControlKind::Pad) ||
                    control.bank > 15 || control.index > 127)
                return false;

            for (uint16_t index = 0; index < noteInputCount; ++index) {
                const APC40ControlId bound = noteInputs[index].control;
                if (bound.kind == control.kind && bound.bank == control.bank && bound.index == control.index) {
                    noteInputs[index].input = input;
                    return true;
                }
            }
            if (noteInputCount >= noteInputCapacity)
                return false;
            noteInputs[noteInputCount++] = {control, input};
            return true;
#else
            (void)control;
            (void)input;
            return false;
#endif
        }

        BaseParameterInput *resolve(APC40ControlId control) const {
            const int slot = slotFor(control);
            if (slot >= 0)
                return inputs[slot];
#ifdef APC40_HAS_NOTE_INPUTS
            if (control.kind != APC40ControlKind::Button && control.kind != APC40ControlKind::Pad)
                return nullptr;
            for (uint16_t index = 0; index < noteInputCount; ++index) {
                const APC40ControlId bound = noteInputs[index].control;
                if (bound.kind == control.kind && bound.bank == control.bank && bound.index == control.index)
                    return noteInputs[index].input;
            }
#endif
            return nullptr;
        }

        void release_note_inputs() {
#ifdef APC40_HAS_NOTE_INPUTS
            for (uint16_t index = 0; index < noteInputCount; ++index) {
                if (noteInputs[index].input)
                    noteInputs[index].input->apply(APC40InputPhase::Release);
            }
#endif
        }
};

#endif