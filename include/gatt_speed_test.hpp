
#include <string>
#include <vector>

#pragma once

/**
 * Perform a write speed test on devices
 * @param devices: vector of names of devices to connect to and test 
 */ 
void GattWriteSpeedTest(std::vector<std::string> device_names);
