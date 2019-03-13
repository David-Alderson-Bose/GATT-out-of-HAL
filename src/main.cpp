/*
 * main.cpp
 * 
 */

#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal
#include <iostream>

// TODO: Figure out getting include into the path
#include "../include/durf.hpp"
#include "android_HAL_ble.hpp"

// ????
//#include "bluetooth.h"


namespace { // Anonymous namespace
    const uint8_t CS_SVC_UUID[] = { 
        0x24, 0xdc, 0x0e, 0x6e, 0x01, 0x40, 0xca, 0x9e,
        0xe5, 0xa9, 0xa3, 0x00, 0xb5, 0xf3, 0x93, 0xe0 };
    const uint8_t CS_CHARACTERISTIC_TX_UUID[] = { 
        0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x40, 0xca, 0x9e, 
        0xe5, 0xa9, 0xa3, 0x00, 0xb5, 0xf3, 0x93, 0xe0  };
    const uint8_t CS_CHARACTERISTIC_RX_UUID[] = { 
        0x24, 0xdc, 0x0e, 0x6e, 0x03, 0x40, 0xca, 0x9e, 
        0xe5, 0xa9, 0xa3, 0x00, 0xb5, 0xf3, 0x93, 0xe0 };
}



// Trying to get some info on how the program exits
struct surround_main {
    surround_main() {std::cout << __func__ << ":Pre-main()!" << std::endl;}
    ~surround_main() {std::cout << __func__ << ":Post-main()!" << std::endl;}
};
static surround_main surroundie;



static volatile sig_atomic_t sig_caught = 0;


void signal_handler(int signum)
{
    std::cout << "Got signal: " << strsignal(signum) << std::endl;
    sig_caught = 1;
}

void wait_for_signal(bool wait=true)
{
    if (!wait) {
        return;
    }
    std::cout << "gonna wait for a signal..." << std::endl;
    pause(); // Holds until signal recieved
}


int main(int argc, char **argv)
{
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);

    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != BTSetup()) {
        std::cout << "No BT for you today!" << std::endl;
        exit(1);
    }

    if (0 != BTConnect("Peripheral_")) {
        std::cout << "blergh, no connectie" << std::endl;
        BTShutdown();
        exit(1);
    }
    wait_for_signal(false);




    if (0 != BLENotifyRegister(CS_CHARACTERISTIC_TX_UUID)) {
        std::cerr << "could not register" << std::endl;
        BTShutdown();
        exit(1);
    }
    sleep(3);

    time_t now = time(nullptr);
    std::string time_str(ctime(&now));
    if (0 != BLEWriteCharacteristic(CS_CHARACTERISTIC_RX_UUID, std::string("hi_im_eddie: ")+time_str)) {
        std::cerr << "COULDN'T WRITE A GOSH DARN THING" << std::endl;
        BTShutdown();
        exit(1);
    }
    //wait_for_signal();

    std::string readie;
    readie = BLEReadCharacteristic(CS_CHARACTERISTIC_TX_UUID);
    if (readie.empty()) {
        std::cerr << "I read nuffin :-(" << std::endl;
    } else {
        std::cout << "Got response: " << readie << std::endl;
    }
    readie = BLEReadCharacteristic(CS_CHARACTERISTIC_RX_UUID);
    if (readie.empty()) {
        std::cerr << "I read nuffin :-(" << std::endl;
    } else {
        std::cout << "Got response: " << readie << std::endl;
    }
    wait_for_signal();

    
    BTShutdown();
    durf("i hope you have...A NICE DAY >:-(");

    return 0;
}

