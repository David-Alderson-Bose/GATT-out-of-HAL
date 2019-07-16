/*
 * main.cpp
 * 
 */

// C headers
#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal 
#include <sys/resource.h> // set priority



// C++ headers
#include <atomic>
#include <ctime>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <map>
#include <vector>



// MY headers
#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>
#include <gatt_speed_test.hpp>

namespace { // Anonymous namespace for connection properties
    // Bud names
    const std::string LEFT_BUD("Rubeus_accel_demo_L");
    const std::string RIGHT_BUD("Rubeus_accel_demo_R");
    const std::string FONE("da_fone");

    // Bud accelerometer axes
    const RivieraBT::UUID BUD_X_AXIS = {
        0xbe, 0x0c, 0x31, 0x50, 0xc9, 0xc2, 0xae, 0x9f, 
        0x44, 0x41, 0x87, 0x06, 0x6e, 0x91, 0x11, 0x89 
    };
    const RivieraBT::UUID BUD_Y_AXIS = {
        0x7d, 0x2e, 0x7f, 0x77, 0x67, 0x23, 0x62, 0xa3, 
        0xc7, 0x44, 0x94, 0x92, 0xdc, 0x87, 0x0f, 0xe5
    }; 
    const RivieraBT::UUID BUD_Z_AXIS = {
        0xde, 0x38, 0xd3, 0x34, 0x48, 0xd3, 0x1a, 0x95, 
        0x12, 0x48, 0xb4, 0x5e, 0x6d, 0x85, 0x0e, 0x60
    };

   
    // Bookkeeping
    static const int CHANNELS(2);
    static const int AXES(3);
    static const int BYTES_PER_READ(2);
    static const int ACCEL_READINGS_SIZE(CHANNELS*AXES*BYTES_PER_READ);
}



static volatile sig_atomic_t sig_caught = 0;
void signal_handler(int signum)
{
    std::cout << "Got signal: " << strsignal(signum) << std::endl;
    sig_caught = 1;
}







int main(int argc, char **argv)
{
    // Printing PID makes it easier to send SIGTERM
    id_t pid = getpid();
    std::cout << "Process ID: " << getpid() << std::endl;
    std::cout << "Setting max process priority..." << std::endl;
    int which = PRIO_PROCESS;
    int priority = -20; // highes priority according to niceness, see https://linux.die.net/man/3/setpriority and https://askubuntu.com/a/1078563
    if (setpriority(which, pid, priority) != 0) {
        std::cerr << "Could not set max priority: " << strerror(errno) << " Why even bother????" << std::endl;
        return -1;
    }
    std::cout << "Max priority set!" << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != RivieraBT::Setup()) {
        std::cout << "No BT for you today!" << std::endl;
        return -1;
    }
    std::cout << "Android HAL BT setup complete" << std::endl;

    // TESTS
    GattWriteSpeedTests::CommandPair(std::vector<std::string>({RIGHT_BUD,LEFT_BUD,}));

    // So long suckers!
    std::cout << std::endl << std::endl;
    RivieraBT::Shutdown();
    std::cout << "bye bye!" << std::endl;
    return 0;
}


