
#include <memory> // in case I want to use shared_ptr
#include <iostream> // for debug print
#include <iomanip> // for extra debug print options
#include <atomic>
#include <sstream>
#include <map>
#include <algorithm> // for equal
#include <iterator>


#include "riviera_bt.hpp"


namespace   // Anonymous namespace for values
{
bluetooth_device_t* s_bt_device;
std::shared_ptr<const bt_interface_t> s_bt_stack;
std::shared_ptr<btgatt_interface_t> s_gatt_interface;
std::atomic_bool s_fluoride_on( false );
}


namespace   // Anonymous namespace for bt_stack callbacks
{
static void AdapterStateChangeCb( bt_state_t state )
{
    //std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Fluoride Stack " << ( ( BT_STATE_ON == state ) ? "enabled" : "disabled" ) << std::endl;
    s_fluoride_on = ( BT_STATE_ON == state );
}

static void AdapterPropertiesCb( bt_status_t status, int num_properties, bt_property_t *properties )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void RemoteDevicePropertiesCb( bt_status_t status, bt_bdaddr_t *bd_addr, int num_properties, bt_property_t *properties )
{
    return;

    // skip all this for now
    std::cout << "Remote Properties from addr 0x";
    for( int i = 0; i < UUID_BYTES_LEN; i++ )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' ) << static_cast<int>( bd_addr->address[i] );
    }
    std::cout << " : " << std::endl;
    for( int entry = 0; entry < num_properties; ++entry )
    {
        std::cout << "\t";
        uint8_t* property_val = static_cast<uint8_t*>( properties[entry].val );
        for( int part = 0; part < ( properties[entry].len ); ++part )
        {
            std::cout << static_cast<int>( property_val[part] );
        }
        std::cout << std::endl;
    }

}

static void DeviceFoundCb( int num_properties, bt_property_t *properties )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void DiscoveryStateChangedCb( bt_discovery_state_t state )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void PinRequestCb( bt_bdaddr_t *bd_addr, bt_bdname_t *bd_name, uint32_t cod, bool min_16_digit )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void SspRequestCb( bt_bdaddr_t *bd_addr, bt_bdname_t *bd_name, uint32_t cod, bt_ssp_variant_t pairing_variant, uint32_t pass_key )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void BondStateChangedCb( bt_status_t status, bt_bdaddr_t *bd_addr, bt_bond_state_t state )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void AclStateChangedCb( bt_status_t status, bt_bdaddr_t *bd_addr, bt_acl_state_t state )
{
    //std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Fluoride Stack acl " << ( ( BT_ACL_STATE_CONNECTED == state ) ? "connected" : "disconnected" ) << std::endl;
}

static void CbThreadEvent( bt_cb_thread_evt event )
{
    //std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Upper level JVM has been " << ( ( event == ASSOCIATE_JVM ) ? "associated" : "disassociated" ) << std::endl;
}

static void DutModeRecvCb( uint16_t opcode, uint8_t *buf, uint8_t len )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void LeTestModeRecvCb( bt_status_t status, uint16_t packet_count )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void EnergyInfoRecvCb( bt_activity_energy_info *p_energy_info, bt_uid_traffic_t *uid_data )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void HCIEventRecvCb( uint8_t event_code, uint8_t *buf, uint8_t len )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}


static bt_callbacks_t sBluetoothCallbacks =
{
    sizeof( sBluetoothCallbacks ),
    AdapterStateChangeCb,
    AdapterPropertiesCb,
    RemoteDevicePropertiesCb,
    DeviceFoundCb,
    DiscoveryStateChangedCb,
    PinRequestCb,
    SspRequestCb,
    BondStateChangedCb,
    AclStateChangedCb,
    CbThreadEvent,
    DutModeRecvCb,
    LeTestModeRecvCb,
    EnergyInfoRecvCb,
    HCIEventRecvCb,
};
}



namespace   // Anonymous namespace for helper functions
{
std::string stringify_UUID( uint8_t* uuid )
{
    if( !uuid )
    {
        std::cerr << __func__ << ": Empty uuid" << std::endl;
        return std::string( "" );
    }
    std::stringstream ss;
    for( int i = UUID_BYTES_LEN - 1; i >= 0; --i )
    {
        ss << std::hex << std::setw( 2 ) << std::setfill( '0' ) << static_cast<int>( uuid[i] );
    }
    return ss.str();
}

pid_t get_other_pid( std::string procname )
{
    // The -s on pidoof means theres MAX 1 result, based off first hit
    // TODO: might be cleaner to do with sprintf or something
    std::string cmd( "pidof -s " );
    cmd.append( procname );
    cmd.append( " 2>/dev/null" ); // direct stderr INTO THE TRASH

    const size_t LEN = 8;
    char line[LEN];

    FILE* pipe = popen( cmd.c_str(), "r" );
    fgets( line, LEN, pipe );
    pid_t pid = strtoul( line, nullptr, 10 );
    pclose( pipe );

    return pid;
}

// Location of unix domain socket to talk with btproperty
// const char* BTPROP_UDS_FILE = "/data/misc/bluetooth/btprop";

// Name of application
const char* BTPROP_PROCESS = "btproperty";

/**
 * Returns btproperty PID if it is running
 * @return btproperty PID if running, negative number if not
 */
pid_t btprop_is_running()
{
    return get_other_pid( BTPROP_PROCESS );
}
}



std::string RivieraBT::StringifyUUID( RivieraBT::UUID uuid )
{
    return stringify_UUID( uuid.data() );
}


std::string RivieraBT::StringifyUUID( bt_uuid_t* uuid )
{
    if( !uuid )
    {
        std::cerr << __func__ << ": Empty bt_uuid_t" << std::endl;
        return std::string( "" );
    }
    return stringify_UUID( ( uuid->uu ) );
}


int RivieraBT::Setup()
{
    if( RivieraBT::isSetup() )
    {
        return 0; // Nothing to do!
    }

    pid_t pid = btprop_is_running();
    if( !pid )
    {
        std::cout << "bt support process not running!" << std::endl;
        return 1;
    }

    int result;
    hw_module_t *hw_module;
    struct hw_device_t *hw_device;

    result = hw_get_module( BT_STACK_MODULE_ID, ( hw_module_t const ** )&hw_module );
    if( result != 0 )
    {
        std::cerr << "hw module could NOT be GOT" << std::endl;
        return 1;
    }

    result = hw_module->methods->open( hw_module, BT_STACK_MODULE_ID, &hw_device );
    if( result != 0 )
    {
        std::cerr << "BT Stack hw module got messed uppppp." << std::endl;
        return 1;
    }

    s_bt_device = reinterpret_cast<bluetooth_device_t*>( hw_device );
    s_bt_stack = std::make_shared<bt_interface_t>( *s_bt_device->get_bluetooth_interface() );


    if( !s_bt_stack )
    {
        RivieraBT::Shutdown();
        std::cout << "IT BLEW UPPPP" << std::endl;
        return 1;
    }
    std::cout << "BT has been set UP." << std::endl;


    // Do I need to reimplement this? or can I leave it?
    result = s_bt_stack->init( &sBluetoothCallbacks ); // if /usr/bin/btproperty is not running, this will exit your program!
    if( result != BT_STATUS_SUCCESS )
    {
        std::cout << "OH CRAP: " << result << std::endl;
        RivieraBT::Shutdown();
        std::cout << "IT BLEW UPPPP" << std::endl;
        return 1;
    }


    s_bt_stack->enable( true );

    // BT STACK NEEDS TO ENABLE BEFORE WE CAN CONTINUE
    // TODO: Make this based on the AdapterStateChangeCb firing
    std::cout << "Waitin for bt to set up before settin up GATT" << std::endl;
    while( !s_fluoride_on );
    std::cout << "DONE WAITIN" << std::endl;

    s_gatt_interface.reset( ( btgatt_interface_t* )s_bt_stack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    if( !s_gatt_interface )
    {
        std::cout << "GRABBIN THE GATT INTERFACE WENT SCREWBALLLZ" << std::endl;
        RivieraBT::Shutdown();
        return 1;
    }
    return 0;
}


bool RivieraBT::isSetup()
{
    return s_fluoride_on;
}


int RivieraBT::Shutdown()
{
    if( s_gatt_interface )
    {
        //s_gatt_interface->cleanup(); // This segfaults
        //s_gatt_interface.reset();    // This also sefgaults!
    }
    if( s_bt_stack )
    {
        s_bt_stack->disable();
        s_bt_stack->cleanup();
        s_bt_stack.reset();
    }
    s_bt_device->common.close( ( hw_device_t* )&s_bt_device->common );
    s_bt_device = NULL;
    std::cout << "Shutdown complete" << std::endl;
    return 0;
}


RivieraBT::GattPtr RivieraBT::GetGatt()
{
    return s_gatt_interface;
}