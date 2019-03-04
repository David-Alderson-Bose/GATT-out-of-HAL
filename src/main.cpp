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


void signal_handler(int signum)
{
    std::cout << "Got signal: " << strsignal(signum) << std::endl;
}


int main(int argc, char **argv)
{
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    BTSetup();


    // Meat of program
    std::cout << "'sup folks" << std::endl;
    std::cout << "gonna wait for a signal..." << std::endl;
    pause();
    
    BTShutdown();

    durf("i hope you have...A NICE DAY >:-(");

    return 0;
}

