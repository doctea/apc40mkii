#include "example_app.h"

#include <iostream>

void ExampleApp::stop() {
    isPlaying = false;
    std::cout << "App state: playback stopped\n";
}

void ExampleApp::play() {
    isPlaying = true;
    std::cout << "App state: playback running\n";
}

void ExampleApp::toggleRecording() {
    isRecording = !isRecording;
    std::cout << "App state: recording " << (isRecording ? "running" : "stopped") << '\n';
}

void ExampleApp::printState() const {
    std::cout << "App state: playback " << (isPlaying ? "running" : "stopped")
              << ", recording " << (isRecording ? "running" : "stopped") << '\n';
}

void ExampleApp::bind_to_apc(APC40Controller& apc) {
    apc.setPlayState(isPlaying);
    apc.setRecordState(isRecording);
    apc.onPlayPressed = [this, &apc] {
        if (isPlaying)
            stop();
        else
            play();
        apc.setPlayState(isPlaying);
    };
    apc.onRecordPressed = [this, &apc] {
        toggleRecording();
        apc.setRecordState(isRecording);
    };

    apc.onTrackKnobChanged = [](APC40TrackKnobBank bank, uint8_t track, uint8_t value) {
        const char* bankName = "Pan";
        if (bank == APC40TrackKnobBank::Sends)
            bankName = "Sends";
        else if (bank == APC40TrackKnobBank::User)
            bankName = "User";
        std::cout << "Input: Track " << (static_cast<int>(track) + 1) << ' ' << bankName
                  << " knob = " << static_cast<int>(value) << '\n';
    };

    apc.onTrackFaderChanged = [](uint8_t track, uint8_t value) {
        std::cout << "Input: Track " << (static_cast<int>(track) + 1)
                  << " fader = " << static_cast<int>(value) << '\n';
    };
    apc.onMasterFaderChanged = [](uint8_t value) {
        std::cout << "Input: Master fader = " << static_cast<int>(value) << '\n';
    };
    apc.onCrossfaderChanged = [](uint8_t value) {
        std::cout << "Input: Crossfader = " << static_cast<int>(value) << '\n';
    };
    apc.onTempoEncoderDelta = [](int16_t delta) {
        std::cout << "Input: Tempo encoder delta = " << static_cast<int>(delta) << '\n';
    };
    apc.onCueEncoderDelta = [](int16_t delta) {
        std::cout << "Input: Cue encoder delta = " << static_cast<int>(delta) << '\n';
    };
}