#include "APC40Controller.h"
#include "APC40RtMidiTransport.h"
#include "example_app.h"
#include <iostream>
#include <thread>
#include <chrono>

#include "constants.h"

int main() {
    APC40RtMidiTransport transport;
    APC40Controller apc(transport);

    if (!apc.init()) {
        std::cerr << "Failed to initialise APC40 MkII" << std::endl;
        return 1;
    }

    std::cout << "APC40 MkII connected." << std::endl;
    ExampleApp app;
    app.bind_to_apc(apc);
    app.printState();

    while (true) {
        apc.loop();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // if (std::cin.peek() != EOF) {
        //     char a = std::cin.get(); // consume the input character
        //     if (a == 'q') {
        //         break; // exit the loop if 'q' is pressed
        //     } else if (a == 'p') {

        //     }
        // }
    }

    return 0;
}
