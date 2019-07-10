/*
 * main.cpp
 * 
 */

// C headers
#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal

// C++ headers
#include <atomic>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>



// MY headers
#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>


namespace { // Anonymous namespace for connection properties
    // Bud names
    const std::string  LEFT_BUD("Rubeus_accel_demo_L");
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

    // Read/Write testing
    const RivieraBT::UUID READ_N_WRITE_UUID = {
        0x80, 0x79, 0x28, 0x8d, 0x83, 0x01, 0xcd, 0xb9, 
        0xa4, 0x49, 0x8f, 0x28, 0xbd, 0x1d, 0x99, 0x6f
    };
    const unsigned int READ_N_WRITE_MTU(32 /*512*/);

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





/**
 * Writes a random string of bytes to the uuid and checks if they have all been written
 * @param connection: gatt client connection
 * @param uuid: uuid to write to and read back from
 * @param len: length of byte string to write
 * @return: zero on success, -1 on write failure, number of incorrect bytes on readback failure
 */ 
int write_n_readback(RivieraGattClient::ConnectionPtr connection, RivieraBT::UUID uuid, unsigned int len = 10)
{
    static const int ASCII_NUMERIC_OFFSET = 48;

    // Write
    /*
    std::string test_str;
    for (int i=0;i<len;++i) {
        int raw_digit = rand() % 10;
        char digit = static_cast<char>(raw_digit+ASCII_NUMERIC_OFFSET);
        test_str.push_back(digit);
    }
    */

    // TODO: Switch to the above
    std::string test_str("hello_from_eddie"); 
   
   
    std::cout << "Testing write & readback with string: " << test_str << std::endl;
    //connection->WriteCharacteristicWhenAvailable(uuid, test_str);
    connection->WriteCharacteristic(uuid, test_str);

    // Readback
    int incorrect_digits = 0;
    std::atomic_bool read_done(false);
    RivieraGattClient::ReadCallback read_cb = [&] (char* buf, size_t length) {
        std::cout << "Get string back: " << std::string(buf, length) << std::endl;
        if (length != test_str.length()) {
            std::cerr << "String length should be " << test_str.length() << " but it's " << length << "!" << std::endl;
            incorrect_digits = test_str.length();
        } else {
            incorrect_digits = strncmp(buf, test_str.c_str(), length);
        }
        read_done = true;
    };
    connection->ReadCharacteristicWhenAvailable(uuid, read_cb);

    // Wait around 
    while (!read_done);
    return incorrect_digits;
}


int main(int argc, char **argv)
{
    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != RivieraBT::Setup()) {
        std::cout << "No BT for you today!" << std::endl;
        exit(1);
    }
    std::cout << "Android HAL BT setup complete" << std::endl;

    std::vector<std::string> devices = {RIGHT_BUD, LEFT_BUD };

    for (auto& device : devices) {
        // Connect
        RivieraGattClient::ConnectionPtr connection = RivieraGattClient::Connect(device);
        if (connection == nullptr) {
            std::cerr << "Could not connect to " << device << "!" << std::endl;
            continue;
        }
        /*
        // Increase MTU
        connection->SetMTU(READ_N_WRITE_MTU);
        connection->WaitForAvailable();
        unsigned int actual_mtu = connection->GetMTU();
        if (READ_N_WRITE_MTU != actual_mtu) {
            std::cerr << "MTU on device " << device << " is " << actual_mtu << 
                " instead of desierved " << READ_N_WRITE_MTU << "!" << std::endl;
        }

        // Write & read
        int incorrect_digits = write_n_readback(connection, READ_N_WRITE_UUID); //, actual_mtu);
        std::cout << "Write & read numeric result: " << incorrect_digits << std::endl;
        */
        connection->WriteCharacteristic(READ_N_WRITE_UUID, "hello from eddie");

        // TODO: Remove this once bugs are gone
        break;
    }

    // So long suckers!
    RivieraBT::Shutdown();
    std::cout << "bye bye!" << std::endl;
    return 0;
}






/*
static int g_accel_readings[ACCEL_READINGS_SIZE] = {0};
std::mutex g_readings_mutex; // This is gonna be callback'd a lot of places, so...
void update_accel_readings(size_t loc, char* buf, size_t len, std::string name=std::string(""))
{
    if (loc%2 != 0) {
        std::cerr << __func__ << ": Only even addresses allowed!" << std::endl;
        return;
    } 
    if (loc+len > ACCEL_READINGS_SIZE) {
        std::cerr << __func__ << ": Data too long for address!" << std::endl;
        return;
    }
    if (len > 2) {
        std::cerr << __func__ << ": Too much data!" << std::endl;
        return;
    }

    // auto-unlocks once out of scope
    std::lock_guard<std::mutex> lock(g_readings_mutex); 

    // Data comes in reversed
    for (int i=0; i<len; ++i) {
        g_accel_readings[loc+len-1-i] = static_cast<int>(buf[i]);
    }
}





void read_accel_data(RivieraGattClient::ConnectionPtr bud, RivieraBT::UUID axis_uuid, size_t position, std::string name=std::string(""))
{
    if (!bud) {
        std::cerr << __func__ << ": need a valid connection!" << std::endl;
        return;
    }

    RivieraGattClient::ReadCallback read_cb = [position, name] (char* buf, size_t len) {
        update_accel_readings(position, buf, len, name);
    };
    if (0 != bud->ReadCharacteristic(axis_uuid, read_cb)) {
        std::cerr << __func__ << ": Couldn't read!" << std::endl;
    }
}


std::string format_accel_readings()
{
    std::ostringstream oss;
    g_readings_mutex.lock();
    for (int i=0; i<ACCEL_READINGS_SIZE; ++i) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(g_accel_readings[i]);
    }
    g_readings_mutex.unlock();
    return oss.str();
}




int main(int argc, char **argv)
{
    // Printing PID makes it easier to send SIGTERM
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if (0 != RivieraBT::Setup()) {
        std::cout << "No BT for you today!" << std::endl;
        exit(1);
    }
    std::cout << "Android HAL BT setup complete" << std::endl;

    // Connect to left bud
    RivieraGattClient::ConnectionPtr left_bud = RivieraGattClient::Connect(LEFT_BUD);
    if (left_bud == nullptr) {
        std::cerr << __func__ << ": Could not connect to " << LEFT_BUD << "!" << std::endl;
        RivieraBT::Shutdown();
        return -1;
    }
    sleep(5); // argh so busted
    
    // Connect to Right bud
    RivieraGattClient::ConnectionPtr right_bud = RivieraGattClient::Connect(RIGHT_BUD);
    if (right_bud == nullptr) {
        std::cerr << __func__ << ": Could not connect to " << RIGHT_BUD << "!" << std::endl;
        RivieraBT::Shutdown();
        return -2;
    }
    sleep(5); // argh so busted

   // Set up signal handling before main loop starts
    signal(SIGINT, signal_handler);

    // Main loop
    int count(0);
    std::cout << "Beginning read loop..." << std::endl;
    while (sig_caught == 0) {
        // Left accel data
        read_accel_data(left_bud, BUD_X_AXIS, 0);
        read_accel_data(left_bud, BUD_Y_AXIS, 2);
        read_accel_data(left_bud, BUD_Z_AXIS, 4);
        

        // Right accel data
        read_accel_data(right_bud, BUD_X_AXIS, 6);
        read_accel_data(right_bud, BUD_Y_AXIS, 8);
        read_accel_data(right_bud, BUD_Z_AXIS, 10);
    
        // WEBSOCKIT TO ME
        std::cout << "Round " << ++count << ": sending accel data " << format_accel_readings() << std::endl;
        //respondo.Respond(1000);
        sleep(1);
    }



    // So long suckers!
    RivieraBT::Shutdown();
    std::cout << "bye bye!" << std::endl;
    return 0;
}
*/









/*
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
*/