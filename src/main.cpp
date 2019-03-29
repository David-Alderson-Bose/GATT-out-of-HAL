/*
 * main.cpp
 * 
 */

#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <ctime>
#include <sstream>

#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>




namespace { // Anonymous namespace
    const RivieraBT::UUID CS_SVC_UUID = { 
        0x24, 0xdc, 0x0e, 0x6e, 0x01, 0x40, 0xca, 0x9e,
        0xe5, 0xa9, 0xa3, 0x00, 0xb5, 0xf3, 0x93, 0xe0 };
    const RivieraBT::UUID CS_CHARACTERISTIC_TX_UUID = { 
        0x24, 0xdc, 0x0e, 0x6e, 0x02, 0x40, 0xca, 0x9e, 
        0xe5, 0xa9, 0xa3, 0x00, 0xb5, 0xf3, 0x93, 0xe0  };
    const RivieraBT::UUID CS_CHARACTERISTIC_RX_UUID = { 
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

void abort_handler(int signum) {
    if (signum != SIGABRT) {
        std::cerr << "DO NOT USE THIS FOR NON_SIGABRYTEuLIULWIUH~~~~!@!!!" << std::endl;
    }
    exit(1);
}



void connect_test() {

    std::unordered_map<std::string, RivieraGattClient::ConnectionPtr> connect_map({
        {"Peripheral_", nullptr},
        {"Rubeus", nullptr}
    });
    for (auto& kv: connect_map) {
        RivieraGattClient::ConnectionPtr connection = RivieraGattClient::Connect(kv.first);
        if (connection == nullptr) {
            std::cerr << __func__ << ": Could not connect to " << kv.first << "!" << std::endl;
            return;
        }
    std::cout << "PAUSED. Send signal (or ctrl+c) to continue" << std::endl;
    pause();
    }
}



void write_n_readback_loop(std::string name, RivieraBT::UUID uuid) 
{
    RivieraGattClient::ConnectionPtr echoer = RivieraGattClient::Connect(name);
    if (echoer == nullptr) {
        std::cerr << __func__ << ": Could not connect to " << name << "!" << std::endl;
        return;
    }
    std::cout << "Beginning write/read loop..." << std::endl;

    std::string recv_str;
    std::atomic_bool read_done(false);
    RivieraGattClient::ReadCallback read_cb = [&] (char* buf, size_t len) {
        recv_str = std::string(buf, len); 
        read_done = true;
    };
    
    while (sig_caught == 0) {     
        std::time_t unix_now = std::time(0);
        std::stringstream ss;
        ss << "hi_im_eddie_" << unix_now;
        std::string send_str(ss.str());

        if (0 != echoer->WriteCharacteristic(uuid, send_str)) {
            std::cerr << "COULDN'T WRITE A GOSH DARN THING" << std::endl;
            continue;
        }
        read_done = false;

        sleep(1);
        if (0 != echoer->ReadCharacteristic(uuid, read_cb)) {
            std::cerr << "I read nuffin :-(" << std::endl;
            continue;
        }
         
        while (!read_done); // wait around
        std:: cout << "xmit results:\n " << "\tSent: " << send_str << "\n\tRead: " << recv_str << 
            "\n\tDiff: " << send_str.compare(recv_str) << std::endl;
    } 
}




int main(int argc, char **argv)
{
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    ///signal(SIGTERM, signal_handler);


    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != RivieraBT::Setup()) {
        std::cout << "No BT for you today!" << std::endl;
        exit(1);
    }
    std::cout << "Android HAL BT setup complete" << std::endl;
 
    write_n_readback_loop("Peripheral_", CS_CHARACTERISTIC_RX_UUID);
    //write_n_readback_loop("Rubeus", CS_CHARACTERISTIC_RX_UUID);
    //connect_test();
    


    RivieraBT::Shutdown();

    std::cout << "bye bye!" << std::endl;
    return 0;
}

