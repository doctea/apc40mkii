#pragma once

#if defined(APC40_ENABLE_PARAMETERS) && APC40_ENABLE_PARAMETERS

#include "APC40Controller.h"
#include "parameter_inputs/MIDIParameterInput.h"

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
        static constexpr int trackFaderOffset = deviceKnobOffset + 8 * 8;
        static constexpr int masterFaderOffset = trackFaderOffset + 8;
        static constexpr int crossfaderOffset = masterFaderOffset + 1;
        static constexpr int cueEncoderOffset = crossfaderOffset + 1;
        BaseParameterInput *inputs[cueEncoderOffset + 1] = {};

        static int slotFor(APC40ControlId control) {
            switch (control.kind) {
                case APC40ControlKind::TrackKnob:
                    return control.bank < 3 && control.index < 8 ? control.bank * 8 + control.index : -1;
                case APC40ControlKind::DeviceKnob:
                    return control.bank < 8 && control.index < 8
                        ? deviceKnobOffset + control.bank * 8 + control.index : -1;
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

        BaseParameterInput *resolve(APC40ControlId control) const {
            const int slot = slotFor(control);
            return slot < 0 ? nullptr : inputs[slot];
        }
};

#endif