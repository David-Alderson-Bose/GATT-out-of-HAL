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

    if (0 != BTConnect(5)) {
        std::cout << "blergh, no connectie" << std::endl;
        BTShutdown();
        exit(1);
    }

    // Meat of program
    std::cout << "'sup folks" << std::endl;
    std::cout << "gonna wait for a signal..." << std::endl;
    pause(); // Holds until signal recieved
    
    BTShutdown();
    durf("i hope you have...A NICE DAY >:-(");

    return 0;
}

