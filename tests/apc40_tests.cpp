#include "APC40BankSet.h"
#include "APC40Controller.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class FakeTransport : public APC40Transport {
public:
    using Message = std::vector<uint8_t>;
    std::vector<Message> sent;
    bool connected = false;

    bool open() override {
        if (!connected) {
            connected = true;
            if (onConnectionChanged)
                onConnectionChanged(true);
        }
        return true;
    }

    void close() override {
        if (connected) {
            connected = false;
            if (onConnectionChanged)
                onConnectionChanged(false);
        }
    }

    bool isConnected() const override { return connected; }

    bool sendMessage(const uint8_t* data, std::size_t size) override {
        if (!connected)
            return false;
        sent.emplace_back(data, data + size);
        return true;
    }

    void receive(Message message) {
        if (connected && onMessage)
            onMessage(message.data(), message.size());
    }
};

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void expectMessage(const FakeTransport& transport, std::size_t index, FakeTransport::Message message) {
    require(index < transport.sent.size(), "Missing MIDI feedback message");
    if (transport.sent[index] != message) {
        std::ostringstream details;
        details << "Incorrect MIDI feedback bytes at " << index << ": expected";
        for (uint8_t byte : message)
            details << ' ' << static_cast<int>(byte);
        details << ", got";
        for (uint8_t byte : transport.sent[index])
            details << ' ' << static_cast<int>(byte);
        throw std::runtime_error(details.str());
    }
}

void testFeedbackBytes() {
    FakeTransport transport;
    APC40Device device(transport);
    require(device.open(), "Device initialization failed");
    APC40Feedback feedback(device);
    feedback.setButtonLED(91, 2, 5);
    feedback.setEncoderRing(48, 3, 100);
    expectMessage(transport, 0, {0x92, 91, 5});
    expectMessage(transport, 1, {0xB3, 48, 100});
}

void testBankSelection() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");
    require(controller.bank_sets.size() == 2, "Expected track and device bank sets");
    APC40BankSet* tracks = controller.bank_sets[0];
    transport.sent.clear();
    transport.receive({0x90, 88, 127});
    require(tracks->getCurrentBank() == 1, "Sends button did not select bank 1");
    require(std::string(tracks->getCurrentBankName()) == "Sends", "Incorrect bank name");
    require(controller.bank_sets[1]->getCurrentBank() == 0, "Unrelated bank set changed");
    expectMessage(transport, 0, {0x90, 87, 0});
    expectMessage(transport, 1, {0x90, 88, 1});
    expectMessage(transport, 2, {0x90, 89, 0});
    expectMessage(transport, 3, {0xB0, 48, 0});
    require(transport.sent.size() == 11, "Default ring redraw should only send values");

    transport.sent.clear();
    transport.receive({0x90, 89, 0});
    transport.receive({0x80, 89, 0});
    tracks->setBank(-1);
    tracks->setBank(3);
    require(tracks->getCurrentBank() == 1, "Release or invalid bank changed selection");
    require(transport.sent.empty(), "Release or invalid bank sent feedback");
}

void testBankedValues() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");
    APC40BankSet* tracks = controller.bank_sets[0];
    int calls = 0;
    int changedEncoder = -1;
    int changedBank = -1;
    uint8_t changedValue = 0;
    tracks->onBankedChange = [&](int encoder, int bank, uint8_t value) {
        ++calls;
        changedEncoder = encoder;
        changedBank = bank;
        changedValue = value;
    };

    transport.sent.clear();
    transport.receive({0xB0, 48, 73});
    require(calls == 1 && changedEncoder == 0 && changedBank == 0 && changedValue == 73,
            "Encoder event did not reach the application");
        require(transport.sent.empty(), "Physical encoder input echoed MIDI feedback");

    transport.receive({0x90, 88, 127});
    transport.receive({0xB0, 48, 29});
    require(calls == 2 && changedBank == 1 && changedValue == 29,
            "Encoder event did not use the selected bank");
    transport.sent.clear();
    transport.receive({0x90, 87, 127});
    expectMessage(transport, 3, {0xB0, 48, 73});
    require(calls == 2, "Bank recall notified an application value change");
    transport.sent.clear();
    transport.receive({0x90, 88, 127});
    expectMessage(transport, 3, {0xB0, 48, 29});
}

void testChannelFiltering() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");
    APC40BankSet* tracks = controller.bank_sets[0];
    int calls = 0;
    tracks->onBankedChange = [&](int, int, uint8_t) { ++calls; };
    transport.sent.clear();
    transport.receive({0x91, 88, 127});
    transport.receive({0xB1, 48, 99});
    transport.receive({0xB0, 47, 99});
    transport.receive({0xB0, 56, 99});
    require(tracks->getCurrentBank() == 0, "Wrong-channel button changed bank");
    require(calls == 0, "Wrong-channel or unrelated CC reached the application");
    require(transport.sent.empty(), "Ignored input sent feedback");
}

void testFadersAndRelativeEncoders() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");

    std::vector<std::pair<uint8_t, uint8_t>> trackFaders;
    std::vector<uint8_t> masterFaders;
    std::vector<uint8_t> crossfaders;
    std::vector<int16_t> tempoDeltas;
    std::vector<int16_t> cueDeltas;
    controller.onTrackFaderChanged = [&](uint8_t track, uint8_t value) {
        trackFaders.emplace_back(track, value);
    };
    controller.onMasterFaderChanged = [&](uint8_t value) { masterFaders.push_back(value); };
    controller.onCrossfaderChanged = [&](uint8_t value) { crossfaders.push_back(value); };
    controller.onTempoEncoderDelta = [&](int16_t delta) { tempoDeltas.push_back(delta); };
    controller.onCueEncoderDelta = [&](int16_t delta) { cueDeltas.push_back(delta); };

    transport.sent.clear();
    for (uint8_t track = 0; track < 8; ++track)
        transport.receive({static_cast<uint8_t>(0xB0 | track), APC40MKII::tracks::CC_FADER,
            static_cast<uint8_t>(track * 16)});
    transport.receive({0xB0, APC40MKII::tracks::CC_FADER_MASTER, 113});
    transport.receive({0xB0, APC40MKII::CC_CROSSFADER, 27});

    const uint8_t relativeValues[] = {0, 1, 63, 64, 65, 127};
    const int16_t expectedDeltas[] = {0, 1, 63, -64, -63, -1};
    for (std::size_t index = 0; index < sizeof(relativeValues); ++index) {
        transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, relativeValues[index]});
        transport.receive({0xB0, APC40MKII::tracks::CC_CUE_LEVEL, relativeValues[index]});
    }

    transport.receive({0xB8, APC40MKII::tracks::CC_FADER, 88});
    transport.receive({0xB1, APC40MKII::tracks::CC_FADER_MASTER, 88});
    transport.receive({0xB1, APC40MKII::CC_CROSSFADER, 88});
    transport.receive({0xB1, APC40MKII::tempo::CC_TEMPO, 64});
    transport.receive({0xB1, APC40MKII::tracks::CC_CUE_LEVEL, 64});

    require(trackFaders.size() == 8, "Track fader channels did not map to all eight tracks");
    for (uint8_t track = 0; track < 8; ++track)
        require(trackFaders[track] == std::make_pair(track, static_cast<uint8_t>(track * 16)),
            "Track fader channel or value was mapped incorrectly");
    require(masterFaders == std::vector<uint8_t>({113}), "Master fader was not routed");
    require(crossfaders == std::vector<uint8_t>({27}), "Crossfader was not routed");
    require(tempoDeltas == std::vector<int16_t>(std::begin(expectedDeltas), std::end(expectedDeltas)),
        "Tempo encoder signed delta decoding failed");
    require(cueDeltas == std::vector<int16_t>(std::begin(expectedDeltas), std::end(expectedDeltas)),
        "Cue encoder signed delta decoding failed");
    require(transport.sent.empty(), "Input-only surface events sent MIDI feedback");
}

void testRelativeEncoderAcceleration() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");

    uint32_t now = 100;
    controller.setMillisProvider([&]() { return now; });
    controller.setTempoEncoderAcceleration({50, 4});
    controller.setCueEncoderAcceleration({25, 3});
    std::vector<int16_t> tempoDeltas;
    std::vector<int16_t> cueDeltas;
    controller.onTempoEncoderDelta = [&](int16_t delta) { tempoDeltas.push_back(delta); };
    controller.onCueEncoderDelta = [&](int16_t delta) { cueDeltas.push_back(delta); };

    transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, 2});
    now = 130;
    transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, 125});
    now = 200;
    transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, 1});

    now = 1000;
    transport.receive({0xB0, APC40MKII::tracks::CC_CUE_LEVEL, 2});
    now = 1010;
    transport.receive({0xB0, APC40MKII::tracks::CC_CUE_LEVEL, 2});
    now = 1040;
    transport.receive({0xB0, APC40MKII::tracks::CC_CUE_LEVEL, 127});

    now = UINT32_MAX - 10;
    transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, 1});
    now = 10;
    transport.receive({0xB0, APC40MKII::tempo::CC_TEMPO, 1});

    require(tempoDeltas == std::vector<int16_t>({2, -12, 1, 1, 4}),
        "Tempo acceleration did not preserve direction, threshold, or wraparound");
    require(cueDeltas == std::vector<int16_t>({2, 6, -1}),
        "Cue acceleration did not use its independent configuration");
}

void testApplicationFeedback() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");
    APC40BankSet* tracks = controller.bank_sets[0];
    int calls = 0;
    tracks->onBankedChange = [&](int, int, uint8_t) { ++calls; };

    transport.sent.clear();
    tracks->setEncoderValue(0, 0, 64);
    expectMessage(transport, 0, {0xB0, 48, 64});
    require(transport.sent.size() == 1, "Active app update sent extra feedback");

    transport.sent.clear();
    tracks->setEncoderValue(0, 1, 90);
    require(transport.sent.empty(), "Inactive app update changed the active ring");
    tracks->setBank(1);
    expectMessage(transport, 3, {0xB0, 48, 90});
    transport.sent.clear();
    tracks->setBank(0);
    expectMessage(transport, 3, {0xB0, 48, 64});

    transport.sent.clear();
    tracks->setEncoderValue(-1, 0, 50);
    tracks->setEncoderValue(8, 0, 50);
    tracks->setEncoderValue(0, -1, 50);
    tracks->setEncoderValue(0, 3, 50);
    require(transport.sent.empty(), "Invalid app update sent feedback");
    tracks->setEncoderValue(0, 0, 255);
    expectMessage(transport, 0, {0xB0, 48, 127});
    require(calls == 0, "App update triggered an incoming-control callback");
}

void testConstBankDefinitions() {
    FakeTransport transport;
    require(transport.open(), "Transport initialization failed");
    APC40Controller controller(transport);
    static const uint8_t buttons[] = {10, 11};
    static const char* const names[] = {"First", "Second"};
    APC40BankSet banks(&controller, "Custom", 2, buttons, names, 16, 2);
    transport.sent.clear();
    banks.setBank(1);
    require(std::string(banks.getCurrentBankName()) == "Second", "Const bank definition failed");
    expectMessage(transport, 0, {0x90, 10, 0});
    expectMessage(transport, 1, {0x90, 11, 1});
    expectMessage(transport, 2, {0xB0, 16, 0});
    expectMessage(transport, 3, {0xB0, 17, 0});
}

void testVirtualEncoderState() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");
    APC40BankSet* tracks = controller.bank_sets[0];

    for (int bank = 0; bank < 3; ++bank) {
        for (int encoder = 0; encoder < 8; ++encoder) {
            APC40VirtualEncoderState state;
            state.value = static_cast<uint8_t>(bank * 32 + encoder);
            state.polarity = (bank + encoder) % 2 == 0
                ? APC40EncoderPolarity::Unipolar : APC40EncoderPolarity::Bipolar;
            state.ringMode = APC40EncoderRingMode::Automatic;
            if (bank == 1 && encoder == 3)
                state.ringMode = APC40EncoderRingMode::Off;
            if (bank == 2 && encoder == 4)
                state.ringMode = APC40EncoderRingMode::Single;
            require(tracks->setEncoderState(encoder, bank, state), "Could not set virtual encoder state");
        }
    }

    for (int bank = 0; bank < 3; ++bank) {
        for (int encoder = 0; encoder < 8; ++encoder) {
            APC40VirtualEncoderState state;
            require(tracks->getEncoderState(encoder, bank, state), "Could not read virtual encoder state");
            require(state.value == bank * 32 + encoder, "Virtual encoder values were shared between slots");
            require(state.polarity == ((bank + encoder) % 2 == 0
                ? APC40EncoderPolarity::Unipolar : APC40EncoderPolarity::Bipolar),
                "Virtual encoder polarity was not retained");
            const APC40EncoderRingMode expectedMode = bank == 1 && encoder == 3
                ? APC40EncoderRingMode::Off
                : bank == 2 && encoder == 4
                    ? APC40EncoderRingMode::Single
                    : APC40EncoderRingMode::Automatic;
            require(state.ringMode == expectedMode, "Explicit ring mode was not retained");
        }
    }

    APC40VirtualEncoderState invalidState;
    require(!tracks->getEncoderState(8, 0, invalidState),
        "Out-of-range virtual encoder state was readable");
    transport.sent.clear();
    tracks->setBank(2);
    expectMessage(transport, 3, {0xB0, 56, 2});
    expectMessage(transport, 4, {0xB0, 48, 64});
    expectMessage(transport, 7, {0xB0, 58, 2});
    expectMessage(transport, 8, {0xB0, 50, 66});
    expectMessage(transport, 11, {0xB0, 60, 1});
    expectMessage(transport, 12, {0xB0, 52, 68});

    require(tracks->setEncoderPolarity(0, 2, APC40EncoderPolarity::Bipolar),
        "Could not update active slot polarity");
    expectMessage(transport, 19, {0xB0, 56, 3});
    expectMessage(transport, 20, {0xB0, 48, 64});
    require(tracks->setEncoderRingMode(0, 2, APC40EncoderRingMode::Single),
        "Could not override automatic ring mode");
    expectMessage(transport, 21, {0xB0, 56, 1});
    expectMessage(transport, 22, {0xB0, 48, 64});
    require(tracks->setEncoderPolarity(0, 2, APC40EncoderPolarity::Unipolar),
        "Could not change polarity with explicit ring mode");
    expectMessage(transport, 23, {0xB0, 56, 1});

    transport.sent.clear();
    tracks->setBank(0);
    require(transport.sent[3] == FakeTransport::Message({0xB0, 56, 2}),
        "Unipolar automatic mode did not select Volume");
    require(transport.sent[5] == FakeTransport::Message({0xB0, 57, 3}),
        "Bipolar automatic mode did not select Pan");
}

void testLogicalTrackKnobAPI() {
    FakeTransport transport;
    APC40Controller controller(transport);
    require(controller.init(), "Controller initialization failed");

    const APC40VirtualEncoderState panState = {
        23, APC40EncoderPolarity::Bipolar, APC40EncoderRingMode::Pan
    };
    const APC40VirtualEncoderState sendsState = {
        71, APC40EncoderPolarity::Unipolar, APC40EncoderRingMode::Volume
    };
    const APC40VirtualEncoderState userState = {
        109, APC40EncoderPolarity::Bipolar, APC40EncoderRingMode::Single
    };
    require(controller.setTrackKnobState(APC40TrackKnobBank::Pan, 3, panState),
        "Could not set logical Pan knob state");
    require(controller.setTrackKnobState(APC40TrackKnobBank::Sends, 3, sendsState),
        "Could not set logical Sends knob state");
    require(controller.setTrackKnobState(APC40TrackKnobBank::User, 3, userState),
        "Could not set logical User knob state");

    APC40VirtualEncoderState actualState;
    require(controller.getTrackKnobState(APC40TrackKnobBank::Pan, 3, actualState) &&
        actualState.value == panState.value && actualState.polarity == panState.polarity &&
        actualState.ringMode == panState.ringMode, "Logical Pan knob state was not retained");
    require(controller.getTrackKnobState(APC40TrackKnobBank::Sends, 3, actualState) &&
        actualState.value == sendsState.value && actualState.polarity == sendsState.polarity &&
        actualState.ringMode == sendsState.ringMode, "Logical Sends knob state was not independent");
    require(controller.getTrackKnobState(APC40TrackKnobBank::User, 3, actualState) &&
        actualState.value == userState.value && actualState.polarity == userState.polarity &&
        actualState.ringMode == userState.ringMode, "Logical User knob state was not independent");
    require(!controller.getTrackKnobState(static_cast<APC40TrackKnobBank>(3), 3, actualState) &&
        !controller.setTrackKnobState(APC40TrackKnobBank::Pan, 8, panState),
        "Invalid logical knob address was accepted");

    APC40TrackKnobBank changedBank = APC40TrackKnobBank::Pan;
    uint8_t changedTrack = 0;
    uint8_t changedValue = 0;
    controller.onTrackKnobChanged = [&](APC40TrackKnobBank bank, uint8_t track, uint8_t value) {
        changedBank = bank;
        changedTrack = track;
        changedValue = value;
    };
    transport.receive({0x90, APC40MKII::tracks::NOTE_BUTTON_USER, 127});
    transport.receive({0xB0, static_cast<uint8_t>(APC40MKII::tracks::CC_KNOB_1 + 3), 44});
    require(changedBank == APC40TrackKnobBank::User && changedTrack == 3 && changedValue == 44,
        "Logical knob callback did not identify its bank and track");
    require(controller.getTrackKnobState(APC40TrackKnobBank::User, 3, actualState) &&
        actualState.value == 44 && actualState.polarity == userState.polarity &&
        actualState.ringMode == userState.ringMode,
        "Physical turn did not update only the addressed logical knob value");
}

void testMessageDecoding() {
    FakeTransport transport;
    APC40Device device(transport);
    require(device.open(), "Device initialization failed");
    int presses = 0;
    int releases = 0;
    int changes = 0;
    int sysex = 0;
    device.onNoteOn = [&](uint8_t note, uint8_t channel, uint8_t value) {
        require(note == 91 && channel == 2 && value == 127, "Incorrect note callback ordering");
        ++presses;
    };
    device.onNoteOff = [&](uint8_t note, uint8_t channel, uint8_t) {
        require(note == 91 && channel == 2, "Incorrect release identity");
        ++releases;
    };
    device.onControlChange = [&](uint8_t cc, uint8_t channel, uint8_t value) {
        require(cc == 48 && channel == 3 && value == 64, "Incorrect CC callback ordering");
        ++changes;
    };
    device.onSystemExclusive = [&](const uint8_t*, std::size_t size) {
        require(size == 4, "Incorrect SysEx message size");
        ++sysex;
    };
    transport.receive({0x92, 91, 127});
    transport.receive({0x82, 91, 127});
    transport.receive({0x92, 91, 0});
    transport.receive({0xB3, 48, 64});
    transport.receive({0xF0, 0x47, 0x29, 0xF7});
    transport.receive({});
    transport.receive({0x92});
    transport.receive({0xB3, 48});
    transport.receive({0xB3, 48, 128});
    transport.receive({0x92, 128, 127});
    transport.receive({0x92, 91, 127, 0});
    transport.receive({0xF0, 0x47});
    transport.receive({0xF8});
    require(presses == 1 && releases == 2 && changes == 1 && sysex == 1,
        "Malformed message dispatched or valid message lost");
}

void testSysExBounds() {
    FakeTransport transport;
    APC40Device device(transport);
    require(device.open(), "Device initialization failed");
    uint8_t payload[249] = {};
    device.send_sysex_command(0x60, 128, payload);
    require(transport.sent.size() == 1 && transport.sent[0].size() == 136, "Incorrect SysEx length");
    require(transport.sent[0][5] == 1 && transport.sent[0][6] == 0 && transport.sent[0].back() == 0xF7,
        "SysEx length must use seven-bit fields");
    device.send_sysex_command(0x60, 248, payload);
    require(transport.sent.size() == 2 && transport.sent[1].size() == 256, "Maximum SysEx rejected");
    device.send_sysex_command(0x60, 249, payload);
    device.send_sysex_command(0x60, -1, payload);
    device.send_sysex_command(0x60, 1, nullptr);
    device.send_sysex_command(0x80, 0, nullptr);
    payload[0] = 128;
    device.send_sysex_command(0x60, 1, payload);
    require(transport.sent.size() == 2, "Invalid SysEx was sent");
    device.send_sysex_command(0x60, 0, nullptr);
    expectMessage(transport, 2, {0xF0, 0x47, 0x7F, 0x29, 0x60, 0, 0, 0xF7});
    transport.close();
    device.sendCC(48, 64, 0);
    device.send_sysex_command(0x60, 0, nullptr);
    require(transport.sent.size() == 3, "Disconnected device sent feedback");
}

void testConnectionResynchronization() {
    FakeTransport transport;
    APC40Controller controller(transport, APC40MKII::sysex::MODE_2);
    require(controller.init(), "Controller initialization failed");
    const FakeTransport::Message introduction = {0xF0, 0x47, 0x7F, 0x29, 0x60, 0, 4, 0x42, 9, 7, 1, 0xF7};
    expectMessage(transport, 0, introduction);
    const auto controlCount = controller.controls.size();
    transport.sent.clear();
    require(controller.init(), "Repeated initialization failed");
    require(controller.bank_sets.size() == 2 && controller.controls.size() == controlCount,
        "Repeated initialization duplicated controls");
    require(transport.sent.empty(), "Repeated initialization replayed feedback");

    auto* tracks = controller.bank_sets[0];
    tracks->setEncoderValue(0, 0, 73);
    tracks->setEncoderValue(0, 1, 29);
    tracks->setBank(1);
    auto* play = controller.bind_note(91, 0);
    play->setAppValue(1);
    int callbacks = 0;
    tracks->onBankedChange = [&](int, int, uint8_t) { ++callbacks; };
    play->onAPCValueChanged = [&](float) { ++callbacks; };
    transport.close();
    transport.sent.clear();
    tracks->setEncoderValue(0, 1, 42);
    require(transport.sent.empty(), "Offline app update sent MIDI");
    require(transport.open(), "Reconnection failed");
    expectMessage(transport, 0, introduction);
    expectMessage(transport, 1, {0x90, 87, 0});
    expectMessage(transport, 2, {0x90, 88, 1});
    expectMessage(transport, 4, {0xB0, 48, 42});
    require(tracks->getCurrentBank() == 1 && play->value == 1, "Reconnect lost application state");
    bool restoredPlay = false;
    for (const auto& message : transport.sent)
        restoredPlay = restoredPlay || message == FakeTransport::Message({0x90, 91, 3});
    require(restoredPlay && callbacks == 0, "Reconnect lost Play LED or replayed a user action");
    transport.sent.clear();
    tracks->setBank(0);
    expectMessage(transport, 3, {0xB0, 48, 73});
}

int main() {
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"feedback bytes", testFeedbackBytes},
        {"bank selection", testBankSelection},
        {"banked values and recall", testBankedValues},
        {"channel filtering", testChannelFiltering},
        {"faders and relative encoders", testFadersAndRelativeEncoders},
        {"relative encoder acceleration", testRelativeEncoderAcceleration},
        {"application feedback", testApplicationFeedback},
        {"const bank definitions", testConstBankDefinitions},
        {"independent virtual encoder state", testVirtualEncoderState},
        {"logical track knob API", testLogicalTrackKnobAPI},
        {"message decoding and malformed input", testMessageDecoding},
        {"SysEx lengths and bounds", testSysExBounds},
        {"startup and reconnect state", testConnectionResynchronization},
    };

    int failures = 0;
    for (const Test& test : tests) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}