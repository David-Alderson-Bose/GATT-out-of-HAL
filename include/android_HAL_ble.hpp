#pragma once

#include <string>


#define UUID_BYTES_LEN 16


int BTSetup();


/**
 * Connect to bluetooth peripheral
 * @param name: name of device to connect to. 
 * @param exact_match: If false, will connect to the first found device that starts with 'name'
 * @param timeout: timeout after this many seconds. Set negative for no timeout.
 * @return: Non-zero on failure, zero on success.
 * TODO: needs arguments for what to connect to (presently hardcoded internally)
 */ 
int BTConnect(std::string name, bool exact_match=false, int timeout=-1);


/**
 * Write to a characteristic of the connected device
 * @param uuid: 16-byte uuid of characteristic
 * @return: Non-zero on faulure, zero on success
 */
int BLEWriteCharacteristic(const uint8_t uuid[UUID_BYTES_LEN], std::string to_write);

/**
 * Read from a characteristic of the connected device
 * @param uuid: 16-byte uuid of characteristic
 * @return: string containing read data, empty on failure
 */
std::string BLEReadCharacteristic(const uint8_t uuid[UUID_BYTES_LEN]);


/**
 * TODO: fill this out
 */
int BLENotifyRegister(const uint8_t uuid[16]);


int BTShutdown();
