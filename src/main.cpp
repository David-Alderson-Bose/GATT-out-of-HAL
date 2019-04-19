/*
 * main.cpp
 * 
 */

#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal
#include <iostream>
#include <iomanip>
#include <atomic>
#include <unordered_map>
#include <ctime>
#include <sstream>

#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>




namespace { // Anonymous namespace for connection properties

    // Left bud name
    const std::string LEFT_BUD("Rubeus_accel_demo_L");

    // Bud accelerometer X-axis
    const RivieraBT::UUID X_AXIS = {
        0xbe, 0x0c, 0x31, 0x50, 0xc9, 0xc2, 0xae, 0x9f, 
        0x44, 0x41, 0x87, 0x06, 0x6e, 0x91, 0x11, 0x89 
    };
}



static volatile sig_atomic_t sig_caught = 0;
void signal_handler(int signum)
{
    std::cout << "Got signal: " << strsignal(signum) << std::endl;
    sig_caught = 1;
}







void read_loop(std::string name, RivieraBT::UUID uuid) 
{
    RivieraGattClient::ConnectionPtr read_only = RivieraGattClient::Connect(name);
    if (read_only == nullptr) {
        std::cerr << __func__ << ": Could not connect to " << name << "!" << std::endl;
        return;
    }
    std::cout << "Beginning read loop..." << std::endl;

    std::string recv_str;
    std::atomic_bool read_done(false);
    RivieraGattClient::ReadCallback read_cb = [&] (char* buf, size_t len) {
        std::ostringstream oss;
        for (int i=len-1; i>=0; --i) {
            oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
        }
        recv_str = oss.str(); 
        read_done = true;
    };
    
    int count(0);
    while (sig_caught == 0) {        
        sleep(1);
        std::cout << "Round " << count++ << ": ";
        read_done = false;
        if (0 != read_only->ReadCharacteristic(uuid, read_cb)) {
            std::cout << "I read nuffin :-(" << std::endl;
            continue;
        }
         
        while (!read_done); // wait around
        std:: cout << "got string '" << recv_str << "'" << std::endl;
    } 
}



int main(int argc, char **argv)
{
    // Set up signal handling
    signal(SIGINT, signal_handler);

    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != RivieraBT::Setup()) {
        std::cout << "No BT for you today!" << std::endl;
        exit(1);
    }
    std::cout << "Android HAL BT setup complete" << std::endl;
 
    read_loop(LEFT_BUD, X_AXIS);


    RivieraBT::Shutdown();
    std::cout << "bye bye!" << std::endl;
    return 0;
}

