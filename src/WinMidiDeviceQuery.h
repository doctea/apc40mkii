#pragma once
#include <string>
#include <vector>

struct MidiDeviceInfo {
    std::string name;
    std::string hardwareId;
    int winmmIndex;
};

std::vector<MidiDeviceInfo> enumerateMidiDevices();
