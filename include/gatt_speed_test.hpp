
#include <string>
#include <vector>

#pragma once



namespace GattWriteSpeedTests {  

    // Default number of writes
    static const unsigned int NUMBER_OF_WRITES(100);

    /**
     * Perform a write speed test on devices
     * @param devices: vector of names of devices to connect to and test 
     * @param number_of_writes: number of times to write 
     */
    void CommandVsRequest(std::vector<std::string> device_names, unsigned int number_of_writes = NUMBER_OF_WRITES);

    
    /**
     * Perform a write speed test on a pair of devices with each device having a thread for writes
     * @param devices: vector of names of devices to connect to and test
     * @param number_of_writes: number of times to write 
     */
    void CommandPairThreaded(std::vector<std::string> device_names, unsigned int number_of_writes = NUMBER_OF_WRITES);
 

    /**
     * Perform a write speed test on a pair of devices. Alternates device every write.
     * @param devices: vector of names of devices to connect to and test
     * @param writes_per_device: number of times to write to each device 
     */
    void CommandPairInterleaved(std::vector<std::string> device_names, unsigned int writes_per_device = NUMBER_OF_WRITES);
}   