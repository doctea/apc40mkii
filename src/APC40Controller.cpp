#include "APC40Controller.h"
#include "APC40BankSet.h"

#include "constants.h"

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

    device.onControlChange = [&](uint8_t cc, uint8_t channel, uint8_t val) {
        for (auto* bs : bank_sets) {
            bs->handleControlChange(cc, val, channel);
        }
        
        for (auto* ctrl : controls) {
            if (ctrl == nullptr) continue;
            if (ctrl->type==APCControlType::Encoder && ctrl->midiCC == cc && ctrl->midiChannel == channel) {
                ctrl->applyAPCEvent(val);
            }
        }
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
        "Pan",
        "Sends",
        "User"
    };
    bank_sets.add(new APC40BankSet(
        this, "Track Knobs", 3,
        track_buttons,
        track_labels,
        APC40MKII::tracks::CC_KNOB_1, 8
    ));

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

