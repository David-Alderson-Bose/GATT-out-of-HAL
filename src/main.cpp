// C headers
#include <unistd.h>  // for pause & getpid
#include <signal.h>
#include <string.h>  // for strsignal 
#include <sys/resource.h> // set priority



// C++ headers
#include <atomic>
#include <ctime>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <map>
#include <vector>



// MY headers
#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>
#include <gatt_speed_test.hpp>

namespace   // Anonymous namespace for connection properties
{

// Remote characteristic UUIDs
const RivieraBT::UUID remote_name =                     //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID appearance =                      //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID service_changed =                 //["Indicate"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x05, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID alert_level =                     //["Read","Write", "Write No Response"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x06, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID tx_power_level =                  //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x07, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID battery_level =                   //["Read", "Notify"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x19, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID system_id =                       //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x23, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID model_number =                    //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x24, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID serial_number =                   //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x25, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID firmware_revision =               //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x26, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID hardware_version =                //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x27, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID software_version =                //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x28, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID manufacturer_name =               //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x29, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID IEEE_reg =                        //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x2a, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID HID_info =                        //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x4a, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID report_map =                      //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x4b, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID HID_control_point =               //["Write No Response"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x4c, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID report =                          //["Read", "Notify", "Write No Response"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x4d, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID PnP_id =                          //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x50, 0x2a, 0x00, 0x00
};
const RivieraBT::UUID central_address_resolution =      //["Read"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xa6, 0x2a, 0x00, 0x00
};

const RivieraBT::UUID CCCdescriptor =                   //["Read", "Write"]
{
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x02, 0x29, 0x00, 0x00
};
// Bookkeeping
static const int CHANNELS( 2 );
static const int AXES( 3 );
static const int BYTES_PER_READ( 2 );
static const int ACCEL_READINGS_SIZE( CHANNELS*AXES*BYTES_PER_READ );
}

static volatile sig_atomic_t sig_caught = 0;
void signal_handler( int signum )
{
    std::cout << "Got signal: " << strsignal( signum ) << std::endl;
    sig_caught = 1;
}

int main( int argc, char **argv )
{
    // Printing PID makes it easier to send SIGTERM
    id_t pid = getpid();
    std::cout << "Process ID: " << getpid() << std::endl;
    std::cout << "Setting max process priority..." << std::endl;
    int which = PRIO_PROCESS;
    int priority = -20; // highes priority according to niceness, see https://linux.die.net/man/3/setpriority and https://askubuntu.com/a/1078563
    if( setpriority( which, pid, priority ) != 0 )
    {
        std::cerr << "Could not set max priority: " << strerror( errno ) << " Why even bother????" << std::endl;
        return -1;
    }
    std::cout << "Max priority set!" << std::endl;

    // What's this?? A BLUETOOTH CALLLLL?????? :-O
    if( 0 != RivieraBT::Setup() )
    {
        std::cout << "No BT for you today!" << std::endl;
        return -1;
    }
    std::cout << "Android HAL BT setup complete" << std::endl;

    // TESTS
    RivieraGattClient::ConnectionPtr connection = RivieraGattClient::Connect( "Bose GC Remote" );
    std::cout << "remote_name\t" << connection->ReadCharacteristic( remote_name ) << "\n";
    std::cout << "appearance\t" << connection->ReadCharacteristic( appearance ) << "\n";
    std::cout << "service_changed\t" << connection->ReadCharacteristic( service_changed ) << "\n";
    std::cout << "alert_level\t" << connection->ReadCharacteristic( alert_level ) << "\n";
    std::cout << "tx_power_level\t" << connection->ReadCharacteristic( tx_power_level ) << "\n";
    std::cout << "battery_level\t" << connection->ReadCharacteristic( battery_level ) << "\n";
    std::cout << "PnP_id\t" << connection->ReadCharacteristic( PnP_id ) << "\n";
    std::cout << "central_address_resolution\t" << connection->ReadCharacteristic( central_address_resolution ) << "\n";
    std::cout << "HID_info\t" << connection->ReadCharacteristic( HID_info ) << "\n";
    std::cout << "report_map\t" << connection->ReadCharacteristic( report_map ) << "\n";
    std::cout << "HID_control_point\t" << connection->ReadCharacteristic( HID_control_point ) << "\n";
    std::cout << "report\t" << connection->ReadCharacteristic( report ) << "\n";
    std::cout << "system_id\t" << connection->ReadCharacteristic( system_id ) << "\n";
    std::cout << "model_number\t" << connection->ReadCharacteristic( model_number ) << "\n";
    std::cout << "serial_number\t" << connection->ReadCharacteristic( serial_number ) << "\n";
    std::cout << "firmware_revision\t" << connection->ReadCharacteristic( firmware_revision ) << "\n";
    std::cout << "hardware_version\t" << connection->ReadCharacteristic( hardware_version ) << "\n";
    std::cout << "software_version\t" << connection->ReadCharacteristic( software_version ) << "\n";
    std::cout << "manufacturer_name\t" << connection->ReadCharacteristic( manufacturer_name ) << "\n";
    std::cout << "IEEE_reg\t" << connection->ReadCharacteristic( IEEE_reg ) << "\n";
    std::cout << "CCCdescriptor\t" << connection->ReadCharacteristic( CCCdescriptor ) << "\n";

    uint8_t notifOn[] = { 0x01, 0x00 };
    uint8_t temp[] = {0x3};
    //std::cout << connection->WriteCharacteristicWhenAvailable(HID_control_point, std::string((char*)temp), RivieraGattClient::Connection::COMMAND) << " write\n";

    while( 1 )
    {
        std::cout << connection->ReadCharacteristic( remote_name );
    }
    // So long suckers!
    std::cout << std::endl << std::endl;
    RivieraBT::Shutdown();
    std::cout << "bye bye!" << std::endl;
    return 0;
}


