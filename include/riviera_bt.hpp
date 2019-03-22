#pragma once


// Android HAL stuff
extern "C" 
{
#include "bluetooth.h"
#include "hardware.h"
#include "bt_av.h"
#include "bt_gatt.h"
#include "bt_gatt_server.h"
#include "bt_gatt_client.h"
#include "hardware/vendor.h"
}


#include <memory>


#define UUID_BYTES_LEN 16



namespace RivieraBT {

    using UUID = const uint8_t[UUID_BYTES_LEN];
    using GattPtr = std::shared_ptr<btgatt_interface_t>; 
    
    /**
     * Set up android HAL stack for bluetooth
     * @return: zero on success, non-zero on failure
     */     
    int Setup();


    /**
     * Setup done or not?
     * @return: true if done, false otherwise
     */
    bool isSetup();

    /**
     * TODO
     */ 
    GattPtr GetGatt();


    std::string StringifyUUID(UUID uuid);
    std::string StringifyUUID(bt_uuid_t* uuid);
}