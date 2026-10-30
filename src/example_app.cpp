// dummy 'sequencer app' for mocking app behaviour

#include <string>
#include <iostream>

#include "APC40Controller.h"

class ExampleApp {
    bool isPlaying = false;
    bool isRecording = false;

    public:
    void stop() {
        if (isPlaying) {
            std::cout << "Stopping playback" << std::endl;
            isPlaying = false;
        } else {
            std::cout << "Playback is already stopped" << std::endl;
        }
        // dummy stop function
    }
    void play() {
        // dummy play function
        if (!isPlaying) {
            std::cout << "Starting playback" << std::endl;
            isPlaying = true;
        } else {
            std::cout << "Playback is already running" << std::endl;
        }
    }

    void record() {
        if (!isRecording) {
            std::cout << "Starting recording" << std::endl;
            isRecording = true;
        } else {
            std::cout << "Recording is already running" << std::endl;
        }
    }
    void stopRecording() {
        if (isRecording) {
            std::cout << "Stopping recording" << std::endl;
            isRecording = false;
        } else {
            std::cout << "Recording is already stopped" << std::endl;
        }
    }

    void bind_to_apc(APC40Controller &apc) {
        
    }
};