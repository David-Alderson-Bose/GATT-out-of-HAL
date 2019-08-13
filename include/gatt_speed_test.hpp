
#include <string>
#include <vector>

#pragma once



namespace GattWriteSpeedTests
{

// Default number of writes
static const unsigned int NUMBER_OF_WRITES( 100 );

/**
 * Perform a write speed test on devices
 * @param devices: vector of names of devices to connect to and test
 */
void CommandVsRequest( std::vector<std::string> device_names, unsigned int number_of_writes = NUMBER_OF_WRITES );


/**
 * Perform a write speed test a pair of devices
 * @param devices: vector of names of devices to connect to and test
 */
void CommandPair( std::vector<std::string> device_names, unsigned int number_of_writes = NUMBER_OF_WRITES );

}