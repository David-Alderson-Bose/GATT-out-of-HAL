# GATT-out-of-HAL
GATT client software for Bose Riviera



## Prerequisites

Before starting any process that needs to use Bluetooth/BLE, make sure the process `btproperty` is running. It can easily be started from the command line, but it is recommended to run it in the background.

## Android HAL interface

The Android HAL is the Qualcomm-recommended API for controlling Bluetooth and BLE. Access to hardware functionality is granted through C function calls that return pointers to structs representing specific portions of hardware functionality. In order to gain access to GATT functionality, one must include the following android HAL headers:

`"hardware.h"`
`"bluetooth.h"`
`"bt_gatt.h"`
`"bt_gatt_server.h"`
`"bt_gatt_client.h"`

If including any of them in a C++ header or source file, you must wrap them in an `extern "C"` block.

### Using GATT via the Android HAL

TODO

Test program "gatt_outa_here"

connection design

