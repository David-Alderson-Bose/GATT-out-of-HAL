
// C headers
#include <string.h>  // for strncmp
#include <unistd.h>  // for sleep

// C++ Headers
#include <chrono>
#include <iostream>
#include <map>
#include <vector>
#include <atomic>

#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>


namespace { // put most of this in an anonymous namespace

    // TODO: Centralize these
    const std::string LEFT_BUD("Rubeus_accel_demo_L");
    const std::string RIGHT_BUD("Rubeus_accel_demo_R");
    
    // Speed test endpoints
    const RivieraBT::UUID READ_N_WRITE_COMMAND = {
        0x80, 0x79, 0x28, 0x8d, 0x83, 0x01, 0xcd, 0xb9, 
        0xa4, 0x49, 0x8f, 0x28, 0xbd, 0x1d, 0x99, 0x6f
    };
    const RivieraBT::UUID READ_N_WRITE_REQUEST = { 
        0xec, 0x56, 0x0c, 0x8c, 0x24, 0x8d, 0x25, 0xb1, 
        0x2f, 0x42, 0x4f, 0xf8, 0x38, 0x8b, 0x72, 0x6f 
    };

    // Speed test verification
    const RivieraBT::UUID WRITE_COMMAND_COUNT = {
        0x7e, 0xfd, 0xc1, 0x64, 0x8c, 0xfc, 0xb1, 0x8c, 
        0xc6, 0x40, 0x1f, 0x84, 0x9e, 0x1b, 0xf9, 0x77
    };
    const RivieraBT::UUID WRITE_REQUEST_COUNT = {
        0xe3, 0x09, 0x8d, 0xe4, 0x2a, 0xb8, 0x03, 0x90, 
        0x4d, 0x4a, 0xb1, 0x0c, 0x22, 0xdb, 0x9b, 0xe9
    };

    // Connection metrics
    const RivieraBT::UUID PDU_ON_READ = { 
        0x4b, 0xc6, 0x50, 0x56, 0x1a, 0x26, 0xda, 0xaf, 
        0xfb, 0x45, 0xab, 0x28, 0xd1, 0x01, 0xc4, 0x00 
    };
    const RivieraBT::UUID PDU_ON_WRITE = { 
        0x3f, 0x78, 0xdb, 0x4d, 0xe6, 0x55, 0x43, 0xa9, 
        0x62, 0x47, 0x7c, 0x4b, 0x66, 0xb4, 0xfa, 0xa8 
    };
    const RivieraBT::UUID CONNECTION_MTU = {
        0xf8, 0xe9, 91, 0x28, 0x8f, 0xe0, 0x16, 0x8c, 
        0x8e, 0x4d, 0x71, 0xd2, 0x13, 0x2a, 0x6a, 0x21
    };
    const RivieraBT::UUID CONNECTION_INVERVAL = {
        0x57, 0x51, 0x31, 0x86, 0x76, 0x63, 0xc7, 0x8b, 
        0xda, 0x47, 0xf4, 0x8c, 0xd5, 0x71, 0x95, 0xb5 
    };
    const std::string CONNECTION_INTERVAL_NAME = "Connection interval";


        using metrics_list = std::vector<std::pair<std::string, RivieraBT::UUID>>;

    const unsigned int READ_N_WRITE_MTU(/*128*//*192*//*224*/255/*256/*512*/);
    const unsigned int HEADER_SIZE(3);
    const unsigned int READ_N_WRITE_MSG_SIZE(READ_N_WRITE_MTU-HEADER_SIZE); // Need three bytes for header stuff



// TODO: Put this and other test utils in their own source/header pair
std::string random_digits(unsigned int length) {
    static const int ASCII_NUMERIC_OFFSET = 48;
    std::string str;
    for (int i=0;i<length;++i) {
        int raw_digit = rand() % 10;
        char digit = static_cast<char>(raw_digit+ASCII_NUMERIC_OFFSET);
        str.push_back(digit);
    }
    return str;
}


/**
 * Writes a random string of bytes to the uuid and checks if they have all been written
 * @param connection: gatt client connection
 * @param uuid: uuid to write to and read back from
 * @param len: length of byte string to write
 * @return: zero on success, -1 on write failure, number of incorrect bytes on readback failure
 */ 
int write_n_readback(RivieraGattClient::ConnectionPtr connection, 
                     RivieraBT::UUID uuid, 
                     RivieraGattClient::Connection::WriteType type,
                     unsigned int len=READ_N_WRITE_MSG_SIZE)
{
    if (!connection) {
        std::cerr << __func__ << ": ConnectionPtr is null!" << std::endl;
        return -1;
    }
    
    // Write
    std::string test_str = random_digits(len); 
    std::cout << "Testing write & readback with string: " << test_str << std::endl;
    connection->WriteCharacteristicWhenAvailable(uuid, test_str, type);

    // Readback
    int incorrect_digits = 0;
    std::atomic_bool read_done(false);
    RivieraGattClient::ReadCallback read_cb = [&] (char* buf, size_t length) {
        //std::cout << "Get string back: " << std::string(buf, length) << std::endl;
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


int write_spam(RivieraGattClient::ConnectionPtr connection, 
               RivieraBT::UUID uuid, 
               RivieraGattClient::Connection::WriteType type, 
               unsigned int len=READ_N_WRITE_MSG_SIZE, 
               unsigned int writes=100)
{
    if (!connection) {
        std::cerr << __func__ << ": ConnectionPtr is null!" << std::endl;
        return -1;
    }

    // Prep
    std::string test_str = random_digits(len); 
    std::cout << "Testing write spam with randomly generated string: " << test_str << std::endl;
    
    // Time the writes
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int rounds=0;rounds<writes;++rounds) {
        // TODO: should we wait for congestion?
        connection->WaitForUncongested();
        connection->WriteCharacteristicWhenAvailable(uuid, test_str, type);
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    // K we're done
    unsigned int duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    float duration_s = static_cast<float>(duration_ms) / 1000.0;
    unsigned int total_bytes = len * writes;
    float bytes_per_mSec = static_cast<float>(total_bytes) / static_cast<float>(duration_ms); 
    std::cout << "Took " << duration_s << "seconds to write " << len << " bytes " << writes << " times (so " << 
        total_bytes << " bytes total). So " << bytes_per_mSec << " bytes/millisecond." << std::endl;
    return 0;
}


/**
 * Read UUIDs that contain metrics about the connection & print findings to stdout
 * @pparam: 
 * @param
 * @return: zero on success, number of missed UUIDs on failure
 */ 
int get_connection_metrics(RivieraGattClient::ConnectionPtr connection, metrics_list& metrics) {
    int error_count(0);
    for (auto& metric: metrics) {
        // TODO: I don't think I need the 'read done' flag so long as Available() works as expected...
        bool read_done(false);
        RivieraGattClient::ReadCallback read_cb = [&] (char* buf, size_t length) {
            unsigned int value(0);
            //std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!! got msg of length " << length << std::endl;
            for (int i=0;i<length; ++i) {
                value += (buf[i] << (i*8)); // add bitshifted values
            }
            std::cout << "Metric " << metric.first << " value: " << value << std::endl;
            if (metric.first == CONNECTION_INTERVAL_NAME) {
                std::cout << "  (that's " << static_cast<float>(value) *1.25 << " milliseconds)" << std::endl;
            }
            read_done = true;
        };
        if (0 != connection->ReadCharacteristicWhenAvailable(metric.second, read_cb)) {
            std::cerr << __func__ << ": could not read UUID associated with metric " << metric.first << std::endl;
        }
        while (!read_done);
    }
    return error_count;
}



} // End anonymous namespace







void GattWriteSpeedTest(std::vector<std::string> device_names) 
{
    // Extra metrics
    metrics_list connection_metrics = {
        {"PDU for reads", PDU_ON_READ},
        {"PDU for writes", PDU_ON_WRITE},
        {"MTU for connection",CONNECTION_MTU},
        {CONNECTION_INTERVAL_NAME, CONNECTION_INVERVAL},
    };

    // Test containers
    // TODO: tuple is kinda ugly, maybe make a full on testing struct
    /*
    std::vector<std::tuple<std::string, RivieraGattClient::ConnectionPtr, RivieraGattClient::Connection::WriteType, RivieraBT::UUID>> devices = {
        std::make_tuple(LEFT_BUD,  nullptr,  RivieraGattClient::Connection::WriteType::REQUEST, READ_N_WRITE_REQUEST),
        std::make_tuple(LEFT_BUD,  nullptr,  RivieraGattClient::Connection::WriteType::COMMAND, READ_N_WRITE_COMMAND),
        std::make_tuple(RIGHT_BUD, nullptr,  RivieraGattClient::Connection::WriteType::REQUEST, READ_N_WRITE_REQUEST),
        std::make_tuple(RIGHT_BUD, nullptr,  RivieraGattClient::Connection::WriteType::COMMAND, READ_N_WRITE_COMMAND),
    };
    */
    
    // Iterate over device names
    for (auto& device_name: device_names) {
        std::cout << std::endl << std::endl;
        std::cout << "************************************************" << std::endl;
        // Connect
        RivieraGattClient::ConnectionPtr connection = RivieraGattClient::Connect(device_name);
        if (connection == nullptr) {
            std::cerr << "Could not connect to " << device_name << "!" << std::endl;
            continue;
        }

        // TODO: For testing with JEFF
        const int KILLTIME(35);
        std::cout << "~~~~~~~!!!!!!!!!!!!!!! Waiting " << KILLTIME << " seconds" << std::endl;
        for (int i=0;i<KILLTIME;++i) {
            sleep(1);
            std::cout << i << std::endl; 
        }


        // Increase MTU & get connection details
        connection->SetMTU(READ_N_WRITE_MTU);
        connection->WaitForAvailable();
        unsigned int actual_mtu = connection->GetMTU();
        if (READ_N_WRITE_MTU != actual_mtu) {
            std::cerr << "MTU on device " << device_name << " is " << actual_mtu << 
                " instead of desierved " << READ_N_WRITE_MTU << "!" << std::endl;
        }
        unsigned int actual_msg_size = actual_mtu-HEADER_SIZE;
        get_connection_metrics(connection, connection_metrics);

        // Check both write-request and write-command for both
        RivieraBT::UUID write_uuid;
        RivieraBT::UUID write_confirm_uuid;
        using GattWriteType = RivieraGattClient::Connection::WriteType;
        // /for (GattWriteType write_type=static_cast<GattWriteType>(0); write_type < GattWriteType::WRITE_TYPES_END; ++write_type)

        // TODO: int to enum is hacky, do this more cleanly
        for (int write_type=0; write_type<RivieraGattClient::Connection::WriteTypes.size(); ++write_type) {
            std::cout << std::endl << "Write type " << RivieraGattClient::Connection::WriteTypes[write_type] << std::endl;
            if (write_type == static_cast<int>(GattWriteType::REQUEST)) {
                write_uuid = READ_N_WRITE_REQUEST;
                write_confirm_uuid = WRITE_REQUEST_COUNT;
            } else if (write_type == static_cast<int>(GattWriteType::COMMAND)) {
                write_uuid = READ_N_WRITE_COMMAND;
                write_confirm_uuid = WRITE_COMMAND_COUNT;
            } else {
                std::cerr << "This write type not supported by GATT speed write test! Skipping..." << std::endl;
                continue;
            }

            const unsigned int NUMBER_OF_WRITES = 100;
            // TODO: Uncomment
            
            // Write & read
            std::cout << "-------------------------------------------------------------"  << std::endl;
            std::cout << "Checking that writes will go through..." << std::endl;
            int incorrect_digits = write_n_readback(connection, write_uuid, static_cast<GattWriteType>(write_type), actual_msg_size);
            std::cout << "Write & read numeric result: " << incorrect_digits << std::endl;
            if (incorrect_digits != 0) {
                std::cerr << "Non-zero result. That's no good! Check the characteristic and try again, please!" << std::endl;
                continue;
            }
            std::cout << "Looks writable. Proceeding to write spam..." << std::endl;

            // Zero out write counter characteristic
            char zeroes[sizeof(uint32_t)] = {0};
            connection->WriteCharacteristicWhenAvailable(write_confirm_uuid, std::string(zeroes, sizeof(uint32_t)), RivieraGattClient::Connection::WriteType::REQUEST);
            
            // Write spam
            //const unsigned int NUMBER_OF_WRITES = 100;
            std::cout << "-------------------------------------------------------------"  << std::endl;
            write_spam(connection, write_uuid, static_cast<GattWriteType>(write_type), actual_msg_size, NUMBER_OF_WRITES);
            
            
            // Read back successful connections
            // TODO: I really need to just add timeouts to all functions to make WaitForAvailable implicit
            sleep(1); // TODO: so fugly
            connection->WaitForAvailable();
            std::string successful_writes_str = connection->ReadCharacteristic(write_confirm_uuid);
            if (successful_writes_str.length() == 0) {
                std::cerr << "Couldn't read number of successful writes! Something went wrong..." << std::endl;
                continue;
            }
            unsigned int successful_writes(0);
            for (int i=0; i<successful_writes_str.length(); ++i) {
                successful_writes += successful_writes_str.c_str()[i] << (i*8);
            }
            std::cout << static_cast<int>(successful_writes) << " of the " << NUMBER_OF_WRITES << " writes were succcessful" << std::endl;
        }


    }
}


    /*
    // Test loops
    for (auto& device : devices) {
        std::cout << std::endl << std::endl;
        std::cout << "************************************************" << std::endl;

        // Connect
        std::get<1>(device) = RivieraGattClient::Connect(std::get<0>(device));
        if (std::get<1>(device) == nullptr) {
            std::cerr << "Could not connect to " << std::get<0>(device) << "!" << std::endl;
            continue;
        }

        // Increase MTU
        std::get<1>(device)->SetMTU(READ_N_WRITE_MTU);
        std::get<1>(device)->WaitForAvailable();
        unsigned int actual_mtu = std::get<1>(device)->GetMTU();
        if (READ_N_WRITE_MTU != actual_mtu) {
            std::cerr << "MTU on device " << std::get<0>(device) << " is " << actual_mtu << 
                " instead of desierved " << READ_N_WRITE_MTU << "!" << std::endl;
        }
        unsigned int actual_msg_size = actual_mtu-HEADER_SIZE;


        get_connection_metrics(std::get<1>(device), connection_metrics);

        
        // Confirm write type        
        if (std::get<2>(device) == RivieraGattClient::Connection::WriteType::REQUEST) {
            std::cout << "This test is for write-REQUEST!" << std::endl;
        } else if (std::get<2>(device) == RivieraGattClient::Connection::WriteType::COMMAND) {
            std::cout << "This test is for write-COMMAND!" << std::endl;
        } else {
            std::cerr << "Hey you tried to slide a bad write type by me you jerk!" <<
                " I'm gonna skip this test for that." << std::endl;
            continue;
        }

        // Write & read
        std::cout << "Checking that writes will go through..." << std::endl;
        int incorrect_digits = write_n_readback(std::get<1>(device), std::get<3>(device), std::get<2>(device), actual_msg_size);
        std::cout << "Write & read numeric result: " << incorrect_digits << std::endl;
        if (incorrect_digits != 0) {
            std::cerr << "Non-zero result. That's no good! Check the characteristic and try again, please!" << std::endl;
            continue;
        }
        std::cout << "Looks writable. Proceeding to write spam..." << std::endl;

        // Write spam
        write_spam(std::get<1>(device), std::get<3>(device), std::get<2>(device), actual_msg_size, 100);
    }
}
*/