#include <RtMidi.h>
#include <iostream>

int main() {
    RtMidiIn in;
    RtMidiOut out;

    std::cout << "Input ports:\n";
    for (unsigned int i = 0; i < in.getPortCount(); i++)
        std::cout << "  " << i << ": " << in.getPortName(i) << "\n";

    std::cout << "\nOutput ports:\n";
    for (unsigned int i = 0; i < out.getPortCount(); i++)
        std::cout << "  " << i << ": " << out.getPortName(i) << "\n";

    return 0;
}