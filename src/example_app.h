#pragma once

#include "APC40Controller.h"

class ExampleApp {
public:
    void bind_to_apc(APC40Controller& apc);
    void printState() const;

private:
    bool isPlaying = false;
    bool isRecording = false;

    void stop();
    void play();
    void toggleRecording();
};