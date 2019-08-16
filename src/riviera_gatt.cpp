
// TODO: I may not need all of these
#include <algorithm>  // for equal
#include <atomic>
#include <chrono>
#include <iomanip>   // for extra debug print options
#include <iostream>  // for debug print
#include <iterator>
#include <map>
#include <memory>  // in case I want to use shared_ptr
#include <sstream>
#include <unordered_map>

#include <errno.h>   // for errno
#include <string.h>  // for strerror()
#include <time.h>    // for time()
#include <unistd.h>  // for access()

#include "DPrint.h"
#include "riviera_bt.hpp"
#include "riviera_gatt_client.hpp"

namespace
{
DPrint s_logger( "GATT-out-of-HAL" );
}

// Anonymous namespace for constants
namespace
{
const std::map<bt_gatt_db_attribute_type_t, std::string> GATT_TYPES_MAP =
{
    {BTGATT_DB_PRIMARY_SERVICE, "primary service"},
    {BTGATT_DB_SECONDARY_SERVICE, "secondary service"},
    {BTGATT_DB_INCLUDED_SERVICE, "included service"},
    {BTGATT_DB_CHARACTERISTIC, "characteristic"},
    {BTGATT_DB_DESCRIPTOR, "descriptor"}
};

const unsigned int DEFAULT_MTU = 23;  // Industry standards
}  // namespace

struct ConnectionData
{
    std::atomic_bool available;
    std::shared_ptr<btgatt_db_element_t> handles_db;
    int handles_count;
    RivieraGattClient::ConnectionPtr connection;
    RivieraGattClient::ReadCallback read_cb;
    std::map<RivieraBT::UUID, std::vector<int>> handles;
    unsigned int mtu;
    std::atomic_bool congested;
    std::atomic_bool resend_needed;
    std::atomic_int times_congested;
    std::chrono::steady_clock::time_point last_congestion;
    std::chrono::milliseconds congestion_duration;
    ConnectionData()
        : available( false ),
          handles_db( nullptr ),
          handles_count( -1 ),
          connection( nullptr ),
          read_cb( nullptr ),
          mtu( DEFAULT_MTU ),
          congested( false ),
          times_congested( 0 ),
          congestion_duration( std::chrono::milliseconds::zero() )
    {
        // congestion_duration = std::chrono::milliseconds::zero();
    }
};

// Anonymous namespace for data storage
namespace
{

// Hold list of connections
std::unordered_map<int, ConnectionData> s_connections;

// Gatt client interface and tracking
std::shared_ptr<const btgatt_client_interface_t> s_gatt_client_interface;
std::atomic_bool s_client_registered( false );
bt_uuid_t s_client_uuid{};
std::atomic_bool s_scanning( false );

// Connecting is a mess of callbacks and needs some of its own data
std::atomic_bool s_connecting( false );
std::atomic_int s_conn_id( -1 );
// std::shared_ptr<bt_bdaddr_t> s_bda;
bt_bdaddr_t* s_bda;
std::string s_name_to_scan;

// Gatt client support
std::atomic_int s_client_if( -1 );

// Testing
std::atomic_int s_write_attempts_since_start( 0 );
std::atomic_int s_writes_complete_since_start( 0 );
}  // namespace

// Anonymous namespace for GATT callbacks
namespace
{

// Callback that triggers once client is registered
void RegisterClientCallback( int status, int client_if, bt_uuid_t* app_uuid )
{
    std::cout << "Registered! uuid:0x";
    for( int i = 0; i < 16; i++ )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<int>( app_uuid->uu[i] );
    }
    std::cout << " client_if:" << client_if << std::endl << std::flush;
    s_client_if = client_if;
    s_client_registered = true;
}

// Callback when scan results are found
void ScanResultCallback( bt_bdaddr_t* bda, int rssi, uint8_t* adv_data )
{
    static const int ADV_DATA_MAX_LEN = 32;
    // static const uint8_t COMPLETE_LOCAL_NAME_FLAG = 0x09;
    // Search specific stuff
    static const uint8_t ON_SEMI_ID[] {0x93, 0x00, 0x80, '\0'};

    std::string matched_name;
    bool manuf_id_match = false;

    static int count = 1;
    bool time_to_stop = false;
    std::string name;
    uint8_t manuf_id[] {0, 0, 0, '\0'};
    int loc = 0;

    // Accumulate info in a stringstream
    std::stringstream ss;
    ss << std::dec << count << ". ";

    // Parse address
    for( int field = 0; field < 6; ++field )
    {
        ss << std::hex << std::setw( 2 ) << std::setfill( '0' )
           << static_cast<int>( bda->address[field] );
        if( field < 5 )
        {
            ss << ":";
        }
    }

    // Read advertising packet
    while( loc < ADV_DATA_MAX_LEN )
    {
        std::size_t len = adv_data[loc];
        if( len == 0 )
        {
            break;
        }
        ss << std::dec << " | Len: " << len;
        uint8_t type = adv_data[loc + 1];
        ss << " Type: " << std::hex << "0x" << static_cast<int>( type );

        // check if type is shortened local name or complete local name
        if( type == 0x08 || type == 0x09 )
        {
            name.assign( reinterpret_cast<char*>( adv_data ) + loc + 2, len - 1 );
            ss << " Name: " << name;
            // std::cout << name << "\n";
            std::size_t found = name.find( s_name_to_scan );
            ;
            if( found != std::string::npos )
            {
                matched_name = s_name_to_scan;
            }
        }
        else if( type == 0xFF ) // manufacturer ID
        {
            ss << " Manuf. ID: 0x";
            for( int i = 0; i < 3; i++ )
            {
                manuf_id[i] = adv_data[loc + 2 + i];
                ss << std::hex << std::setw( 2 ) << std::setfill( '0' )
                   << static_cast<int>( manuf_id[i] );
            }
            int match = strcmp( reinterpret_cast<const char*>( ON_SEMI_ID ),
                                reinterpret_cast<char*>( manuf_id ) );
            if( match == 0 )
            {
                manuf_id_match = true;
            }
        }
        else
        {
            ss << std::hex << " Data: 0x";
            for( unsigned int i = 1; i < len; ++i )
            {
                ss << std::hex << std::setw( 2 ) << std::setfill( '0' )
                   << static_cast<int>( adv_data[loc + i] );
            }
        }
        loc += ( len + 1 );
    }

    // Only print if we got a name
    // if (!name.empty()) {
    //    std::cout << ss.str() << std::endl;
    //}

    // PACK IT IN, BOYS
    if( !matched_name.empty() && manuf_id_match )
    {
        std::cout << ss.str() << std::endl;
        std::cout << "Found device with name '" << s_name_to_scan << "'!"
                  << std::endl;
        time_to_stop = true;
    }

    // Call back into class to stop scanning
    if( time_to_stop && s_gatt_client_interface )
    {
        s_gatt_client_interface->scan( false );
        // s_bda.reset(bda);
        s_bda = bda;
        s_scanning = false;
    }
    ++count;
}

void ConnectClientCallback( int conn_id, int status, int client_if,
                            bt_bdaddr_t* bda )
{
    std::cout << "CONNECTED, client_if:" << client_if << " conn_id:" << conn_id
              << " address:0x";
    for( int i = 0; i < 16; i++ )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<int>( bda->address[i] );
    }
    std::cout << std::endl;
    s_conn_id = conn_id;
    s_connecting = false;
}

void DisconnectClientCallback( int conn_id, int status, int client_if,
                               bt_bdaddr_t* bda )
{
    std::cout << "D_I_S_C_O_N_N_E_C_T_E_D, client_if:" << client_if
              << ", conn_id:" << std::dec << conn_id << ", uuid:0x";
    BOSE_LOG( INFO, "Disconnected from remote" );
    for( int i = 0; i < UUID_BYTES_LEN; i++ )
    {
        std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' )
                  << static_cast<int>( bda->address[i] );
    }
    std::cout << std::endl;
    s_conn_id = -1;
}

void search_complete_callback( int conn_id, int status )
{
    std::cout << "Service search complete on conn_id " << conn_id
              << " with status " << std::hex << status << std::endl;
    s_scanning = false;
}

void get_gatt_db_callback( int conn_id, btgatt_db_element_t* db, int count )
{
    if( s_connections.count( conn_id ) > 0 )
    {
        std::cout << "!~! DEBUG !~! Conn id " << conn_id
                  << " got GATT db callback with " << count << " handles "
                  << std::endl;
        s_connections[conn_id].handles_db.reset( db );
        s_connections[conn_id].handles_count = count;
        s_connections[conn_id].available = true;
    }
    else
    {
        std::cerr << "GATT db ball back for " << conn_id
                  << " which is not registered!" << std::endl;
    }
}

void write_characteristic_cb( int conn_id, int status, uint16_t handle )
{
    if( s_connections.count( conn_id ) == 0 || !s_connections[conn_id].connection )
    {
        std::cerr << __func__ << ": got bad write callback on unclaimed conn_id "
                  << conn_id;
        return;
    }
    // std::cout << "Write on connection  '" <<
    // s_connections[conn_id].connection->GetName() << "', handle 0x" <<
    //    std::hex << handle << std::dec << " complete with status code " <<
    //    std::hex << status << std::dec << std::endl;
    // std::cout << "!!!~DEBUG~!!! writes completed since program start: " <<
    // ++writes_since_start << std::endl; std::cout << "!!!~DEBUG~!!! writes
    // attempted: " << s_write_attempts_since_start << "   write completed " <<
    // ++s_writes_complete_since_start << std::endl;
    s_connections[conn_id].available = true;
    if( s_connections[conn_id].congested )
    {
        s_connections[conn_id].resend_needed = true;
    }
}

void execute_write_cb( int conn_id, int status )
{
    std::cout << "Write executed on connection '"
              << s_connections[conn_id].connection->GetName() << "' (conn_id "
              << conn_id << " with status 0x" << std::hex << status << std::dec
              << std::endl;
    s_connections[conn_id].available = true;
}

void read_characteristic_cb( int conn_id, int status,
                             btgatt_read_params_t* p_data )
{
    if( s_connections.count( conn_id ) == 0 || !s_connections[conn_id].connection )
    {
        std::cerr << __func__ << ": got bad write callback on unclaimed conn_id "
                  << conn_id;
        return;
    }

    // std::cout << "Read on connection  " <<
    // s_connections[conn_id].connection->GetName() <<
    // " complete with status code " << std::hex << status << std::dec <<
    // std::endl;

    if( !p_data )
    {
        std::cerr << __func__ << ": no data recieved!" << std::endl;
    }
    else
    {
        //    std::cout << __func__ << ": recieved " <<
        //        std::string(reinterpret_cast<char*>(p_data->value.value),
        //        p_data->value.len) << std::endl;
        if( s_connections[conn_id].read_cb )
        {
            s_connections[conn_id].read_cb(
                reinterpret_cast<char*>( p_data->value.value ), p_data->value.len );
            s_connections[conn_id].read_cb = nullptr;  // clear callback
        }
        else
        {
            std::cout << __func__ << ": recieved "
                      << std::string( reinterpret_cast<char*>( p_data->value.value ),
                                      p_data->value.len )
                      << std::endl;
        }
    }
    s_connections[conn_id].available = true;
}

void configure_mtu_callback( int conn_id, int status, int mtu )
{
    if( s_connections.count( conn_id ) == 0 || !s_connections[conn_id].connection )
    {
        std::cerr << __func__ << ": got bad mtu callback on unclaimed conn_id "
                  << conn_id;
        return;
    }
    // s_connections[conn_id].mtu = mtu; // TODO: add back in once status code is
    // observed to work as expected
    std::cout << __func__ << ": MTU for connection ID " << conn_id
              << " negotiated to " << mtu << " with status code " << status
              << std::endl;
    s_connections[conn_id].mtu = mtu;
    s_connections[conn_id].available = true;
}

void register_for_notification_cb( int conn_id, int registered, int status,
                                   uint16_t handle )
{
    // TODO: Read the data & do something with it
    std::cout << __func__ << ": NOTIFICATION RESULT " << registered
              << " ON CONN ID " << conn_id << std::endl;
    s_connections[conn_id].available = true;
}

void notify_cb( int conn_id, btgatt_notify_params_t* p_data )
{
    // TODO: Read the data & do something with it
    std::cout << __func__ << ": DO SOMETHING WITH THE NOTIFICATION ON CONN ID "
              << conn_id << std::endl;
}

void congestion_callback( int conn_id, bool congested )
{
    // std::cerr << "!!!!!! Connection " <<
    // s_connections[conn_id].connection->GetName() << " (id " << conn_id << ")
    // has congestion status: " << congested << std::endl;
    if( congested )
    {
        s_connections[conn_id].times_congested++;
        s_connections[conn_id].last_congestion = std::chrono::steady_clock::now();
        //    std::cerr << "   writes attempted: " << s_write_attempts_since_start
        //    << "   write completed " << s_writes_complete_since_start <<
        //    std::endl;
    }
    else
    {
        auto now = std::chrono::steady_clock::now();
        s_connections[conn_id].congestion_duration +=
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_connections[conn_id].last_congestion );
    }
    s_connections[conn_id].congested = congested;
}

btgatt_client_callbacks_t sBTGATTClientCallbacks =
{
    RegisterClientCallback,
    ScanResultCallback,
    ConnectClientCallback,     // connect_callback
    DisconnectClientCallback,  // disconnect_callback
    search_complete_callback,
    register_for_notification_cb,
    notify_cb,
    read_characteristic_cb,
    write_characteristic_cb,
    NULL,  // read_descriptor_cb,
    NULL,  // write_descriptor_cb,
    execute_write_cb,
    NULL,  // read_remote_rssi_callback
    NULL,  // ListenCallback,
    configure_mtu_callback,
    NULL,  // scan_filter_cfg_callback
    NULL,  // scan_filter_param_callback
    NULL,  // scan_filter_status_callback
    NULL,  // multi_adv_enable_callback
    NULL,  // multi_adv_update_callback
    NULL,  // multi_adv_data_callback
    NULL,  // multi_adv_disable_callback
    congestion_callback,
    NULL,  // batchscan_cfg_storage_callback
    NULL,  // batchscan_enable_disable_callback
    NULL,  // batchscan_reports_callback
    NULL,  // batchscan_threshold_callback
    NULL,  // track_adv_event_callback
    NULL,  // scan_parameter_setup_completed_callback
    get_gatt_db_callback,
    NULL,  // services_removed_callback
    NULL,  // services_added_callback
};
}  // namespace

// Anonymous namespace for gatt server stuff
namespace
{
static btgatt_server_callbacks_t sBTGATTServerCallbacks =
{
    NULL,  // RegisterServerCallback,
    NULL,  // ConnectionCallback,
    NULL,  // ServiceAddedCallback,
    NULL,  // IncludedServiceAddedCallback,
    NULL,  // CharacteristicAddedCallback,
    NULL,  // DescriptorAddedCallback,
    NULL,  // ServiceStartedCallback,
    NULL,  // ServiceStoppedCallback,
    NULL,  // ServiceDeletedCallback,
    NULL,  // RequestReadCallback,
    NULL,  // RequestWriteCallback,
    NULL,  // RequestExecWriteCallback,
    NULL,  // ResponseConfirmationCallback,
    NULL,  // IndicationSentCallback,
    NULL,  // CongestionCallback,
    NULL,  // MtuChangedCallback
};
}

// Anonymous namespace (a cute lil one) for setup of the GATT device
namespace
{
btgatt_callbacks_t s_gatt_callbacks =
{
    sizeof( s_gatt_callbacks ),
    &sBTGATTClientCallbacks,
    &sBTGATTServerCallbacks,
};
}

// Anonymous namespace for static functions
namespace
{
int gatt_setup()
{
    if( s_gatt_client_interface )
    {
        // Already good to go
        return 0;
    }

    // We might need to start from scratch...
    if( !RivieraBT::isSetup() )
    {
        if( RivieraBT::Setup() != 0 )
        {
            std::cerr << __func__ << ": Could not set up riviera Bluetooth!"
                      << std::endl;
            return -1;
        }
    }

    // Initialize GATT portion of BT/BLE stack
    if( BT_STATUS_SUCCESS != RivieraBT::GetGatt()->init( &s_gatt_callbacks ) )
    {
        std::cerr << __func__ << ": Could not register gatt client!" << std::endl;
        return -1;
    }

    // To the registrar!
    s_gatt_client_interface.reset( RivieraBT::GetGatt()->client );
    int registered = s_gatt_client_interface->register_client( &s_client_uuid );
    if( !s_gatt_client_interface || registered != BT_STATUS_SUCCESS )
    {
        std::cerr << __func__ << ": Could not register gatt client!" << std::endl;
        return -1;
    }

    while( !s_client_registered )
        ;  // Kill some time
    std::cout << "GATT setup complete" << std::endl;
    return 0;
}

// int clear_connect_data() {
//     if (s_connecting != false) {
//         return -1;
//     }
//     s_conn_id = -1;
//     //s_bda.reset(); // This causes a segfault!
//     s_bda = nullptr;
//     s_name_to_scan.clear();
//     return 0;
// }
}  // namespace

RivieraGattClient::ConnectionPtr RivieraGattClient::Connect( std::string name,
                                                             bool exact_match,
                                                             int timeout )
{
    if( gatt_setup() != 0 )
    {
        std::cerr << __func__ << ": Gatt setup failed!" << std::endl;
        return nullptr;
    }

    // Wait in case scanning already
    std::cout << "Checking for ongoing scan..." << std::endl;
    while( s_scanning )
        ;

    // Start scan and wait - scan will initiate connection
    std::cout << "Initiating scan for " << name << "..." << std::endl;
    s_name_to_scan = name;
    s_scanning = true;
    s_gatt_client_interface->scan( true );
    while( s_scanning )
        ;  // wait for scan to complete

    // Connect if possible
    if( !s_bda )
    {
        std::cerr << "No record of address for " << name << std::endl;
        return nullptr;
    }
    std::cout << "Attempting to connect to " << name << "..." << std::endl;
    RivieraBT::Bond( s_bda );
    s_conn_id = -1;
    while( s_conn_id <= 0 )
    {
        s_gatt_client_interface->connect( s_client_if, /*s_bda.get()*/ s_bda, true, 0 );
        while( s_conn_id < 0 );
    }

    // Do something else if multiples of same name are allowed
    for( auto& kv : s_connections )
    {
        if( kv.second.connection && kv.second.connection->GetName() == name )
        {
            std::cout << "Already registered " << name
                      << ", so returning pointer to it" << std::endl;
            return kv.second.connection;
        }
    }

    std::cout << "Finalizing connection..." << std::endl;
    s_gatt_client_interface->conn_parameter_update( s_bda, 8, 8, 0, 2000 );
    s_connections[s_conn_id];  // create spot
    Connection* temp = new RivieraGattClient::Connection(
        name, s_conn_id, /*s_bda.get()*/ s_bda, &( s_connections[s_conn_id] ) );
    s_connections[s_conn_id].connection.reset( temp );
    s_connections[s_conn_id].available = true;
    s_connecting = false;
    s_scanning = true;
    s_gatt_client_interface->search_service(
        s_client_if, nullptr ); // Need to do this before get_gatt_db will work
    std::cout << "is it";
    //while( s_scanning )
    //    ;
    std::cout << "here?";
    s_connections[s_conn_id].connection->fetch_services();
    // clear_connect_data();
    return s_connections[s_conn_id].connection;
}

void RivieraGattClient::Connection::Reconnect()
{
    // Wait in case scanning already
    std::cout << "Checking for ongoing scan..." << std::endl;
    while( s_scanning )
        ;

    // Start scan and wait - scan will initiate connection
    std::cout << "Initiating scan for " << m_name << "..." << std::endl;
    s_name_to_scan = m_name;
    s_scanning = true;
    s_gatt_client_interface->scan( true );
    while( s_scanning )
        ;  // wait for scan to complete

    // Connect if possible
    if( !s_bda )
    {
        std::cerr << "No record of address for " << m_name << std::endl;
        return;
    }
    std::cout << "Attempting to connect to " << m_name << "..." << std::endl;
    s_conn_id = -1;
    while( s_conn_id <= 0 )
    {
        s_gatt_client_interface->connect( s_client_if, /*s_bda.get()*/ s_bda, true, 0 );
        while( s_conn_id < 0 );
    }
}

void RivieraGattClient::Connection::Disconnect()
{
    s_gatt_client_interface->disconnect( s_client_if, s_bda, s_conn_id );
    while( s_conn_id > 0 )
        ;
}

int RivieraGattClient::Connection::GetConnectionID()
{
    return m_conn_id;
}

std::string RivieraGattClient::Connection::GetName()
{
    return m_name;
}

unsigned int RivieraGattClient::Connection::GetCongestions()
{
    return static_cast<unsigned int>( m_data->times_congested );
}

unsigned int RivieraGattClient::Connection::GetTimeSpentCongested()
{
    return static_cast<unsigned int>( m_data->congestion_duration.count() );
}

RivieraGattClient::Connection::Connection( std::string name, int conn_id,
                                           bt_bdaddr_t* bda,
                                           ConnectionData* data )
    : m_name( name ), m_conn_id( conn_id ), m_bda( bda )
{
    m_data.reset( data );
    std::cout << "Created connection " << m_name << " with conn_id " << std::dec
              << m_conn_id << std::endl;
    BOSE_LOG( INFO, "Connected to " << m_name );
}

void RivieraGattClient::Connection::PrintHandles()
{
    for( auto& kv : m_data->handles )
    {
        for( auto& temp : kv.second )
        {


            std::cout << "  " << RivieraBT::StringifyUUID( kv.first ) << " : "
                      << temp << std::endl;
        }
        std::cout << std::endl;
    }
}

void RivieraGattClient::Connection::fill_handle_map()
{
    if( !m_data->handles_db || m_data->handles_count <= 0 )
    {
        std::cerr << __func__ << ": no handdles found" << std::endl;
        return;
    }
    for( int entry = 0; entry < ( m_data->handles_count ); ++entry )
    {
        // if (m_data->handles_db.get()[entry].type != BTGATT_DB_CHARACTERISTIC) {
        //    continue;
        //}
        std::cout << "  Adding entry for "
                  << m_data->handles_db.get()[entry].attribute_handle << std::endl;
        RivieraBT::UUID uuid;
        memcpy( uuid.data(), m_data->handles_db.get()[entry].uuid.uu,
                UUID_BYTES_LEN );
        ( m_data->handles[uuid] ).push_back( m_data->handles_db.get()[entry].attribute_handle );
    }
    std::cout << "Filled " << m_data->handles.size()
              << " entries in the handle map" << std::endl;
}

void RivieraGattClient::Connection::fetch_services( void )
{
    std::cout << "Fetching gatt database..." << std::endl;

    for( int retries = 0; retries < 10; ++retries ) // TODO: better retry logic
    {
        m_data->available = false;
        s_gatt_client_interface->get_gatt_db( m_conn_id );
        while( !m_data->available )
            ;
        if( m_data->handles_count > 0 )
        {
            break;
        }
        else
        {
            std::cerr << "Got no handles from handle search! Trying again..."
                      << std::endl;
            sleep( 1 );
        }
    }

    fill_handle_map();
    std::cout << "Finished attempt to fill handle map" << std::endl;
    PrintHandles();
}

/**
 * Find uuid in the db
 * @return: non-zero handle on success
 */
uint16_t RivieraGattClient::Connection::find_characteristic_handle_from_uuid(
    RivieraBT::UUID uuid )
{
    std::cout << __func__ << ": 1" << std::endl;
    if( !m_data->handles_db || m_data->handles_count <= 0 )
    {
        std::cerr << __func__ << ": GATT client db uninitialized" << std::endl;
        return 0;
    }

    // Search
    uint16_t return_handle = 0;
    for( int entry = 0; entry < m_data->handles_count; ++entry )
    {
        std::cout << "entry " << entry << std::endl;
        if( m_data->handles_db.get()[entry].type != BTGATT_DB_CHARACTERISTIC )
        {
            continue;
        }
        uint8_t* device_uuid = m_data->handles_db.get()[entry].uuid.uu;
        if( 0 == memcmp( uuid.data(), device_uuid, UUID_BYTES_LEN ) )
        {
            return_handle = m_data->handles_db.get()[entry].attribute_handle;
            break;
        }
    }
    return return_handle;
}

int RivieraGattClient::Connection::RegisterNotification( RivieraBT::UUID uuid, int index )
{
    int result = s_gatt_client_interface->register_for_notification(
                     s_client_if, s_bda, ( m_data->handles[uuid] )[index] );
    if( BT_STATUS_SUCCESS != result )
    {
        std::cerr << __func__ << ": register FAILED with code " << result
                  << std::endl;
        return -1;
    }
    m_data->available = false;
    return 0;
}

int RivieraGattClient::Connection::DeregisterNotification(
    RivieraBT::UUID uuid, int index )
{
    int result = s_gatt_client_interface->deregister_for_notification(
                     s_client_if, s_bda, ( m_data->handles[uuid] )[index] );
    if( BT_STATUS_SUCCESS != result )
    {
        std::cerr << __func__ << ": deregister FAILED with code " << result
                  << std::endl;
        return -1;
    }
    return 0;
    m_data->available = false;
}

bool RivieraGattClient::Connection::Available()
{
    return m_data->available;
}

int RivieraGattClient::Connection::WaitForAvailable( unsigned int timeout )
{
    // TODO: This is a hack, add a timeout or something
    while( !Available() )
        ;
    return 0;
}

bool RivieraGattClient::Connection::IsCongested()
{
    return m_data->congested;
}

int RivieraGattClient::Connection::WaitForUncongested( unsigned int timeout )
{
    // TODO: This is a hack, add a timeout or something
    while( IsCongested() )
        ;
    return 0;
}

#undef WRITE_TYPE
#define WRITE_TYPE(x) #x,
const std::vector<std::string> RivieraGattClient::Connection::WriteTypes =
{
    WRITE_TYPES
};

int RivieraGattClient::Connection::WriteCharacteristicWhenAvailable(
    RivieraBT::UUID uuid, std::string to_write,
    RivieraGattClient::Connection::WriteType type, unsigned int timeout )
{
    WaitForAvailable( timeout );
    return WriteCharacteristic( uuid, to_write, type );
}

int RivieraGattClient::Connection::WriteCharacteristic(
    RivieraBT::UUID uuid, std::string to_write,
    RivieraGattClient::Connection::WriteType type, int index )
{
    //if( !m_data->available )
    //{
    //    std::cerr << "Connection busy" << std::endl;
    //    return -1;
    //}

    if( m_data->handles.empty() )
    {
        fetch_services();
    }

    std::string uuid_str( RivieraBT::StringifyUUID( uuid ) );
    if( m_data->handles.count( uuid ) == 0 )
    {
        std::cerr << __func__ << "UUID not available in connection" << std::endl;
        return -1;
    }

    uint16_t handle = ( m_data->handles[uuid] )[index];
    std::cout << handle << std::endl;

    int type_numeric;
    if( type == RivieraGattClient::Connection::WriteType::COMMAND )
    {
        type_numeric = 1;
    }
    else if( type == RivieraGattClient::Connection::WriteType::REQUEST )
    {
        type_numeric = 2;
    }
    else
    {
        std::cerr << __func__ << ": Unavailable write type!" << std::endl;
        return -1;
    }

    // third argument is write-type
    // 0 doesn't work
    // 1 is write-command (no reply given at stack level)
    // 2 is write-request (needs reply at stack level)
    // TODO: Make constants for these
    int result = s_gatt_client_interface->write_characteristic(
                     m_conn_id, handle, type_numeric /*1 2*/, to_write.size(), 0,
                     const_cast<char*>( to_write.c_str() ) );
    if( BT_STATUS_SUCCESS != result )
    {
        std::cerr << __func__ << ": Write FAILED with code " << result << std::endl;
        return -1;
    }

    m_data->available = false;
    // std::cout << "Wrote " << to_write << " to UUID " << uuid_str << ", handle
    // 0x" <<
    //    std::hex << static_cast<int>(handle) << std::dec << ". Waiting for
    //    response..." << std::endl;
    // std::cout << "!!!~WRITE~!!! writes attempted: " <<
    // ++s_write_attempts_since_start << "   write completed " <<
    // s_writes_complete_since_start << std::endl;
    return 0;
}

std::string RivieraGattClient::Connection::ReadCharacteristic(
    RivieraBT::UUID uuid , int index )
{
    std::cout << "read";
    std::atomic_bool read_done( false );
    std::string ret_string;
    RivieraGattClient::ReadCallback read_cb = [&]( char* buf, size_t length )
    {
        std::cout << "read_callback";
        ret_string = std::string( buf, length );
        read_done = true;
    };
    int read_return = ReadCharacteristic( uuid, read_cb , index );
    if( read_return != 0 )
    {
        // empty string on failure
        return std::string();
    }
    while( !read_done )
        ;  // Kill time until read is done
    return ret_string;
}

int RivieraGattClient::Connection::ReadCharacteristicWhenAvailable(
    RivieraBT::UUID uuid, ReadCallback cb, unsigned int timeout )
{
    WaitForAvailable( timeout );
    return ReadCharacteristic( uuid, cb );
}

int RivieraGattClient::Connection::ReadCharacteristic( RivieraBT::UUID uuid,
                                                       ReadCallback cb, int index )
{
    // Removing to see if we can get multiple consecutive reads goin
    // if (!m_data->available) {
    //    std::cerr << "Connection busy" << std::endl;
    //    return -1;
    //}
    if( m_data->handles.empty() )
    {
        fetch_services();
    }
    if( m_data->handles.count( uuid ) == 0 || ( m_data->handles[uuid] )[index] <= 0 )
    {
        std::cerr << __func__ << ": Couldn't find handle associated with uuid"
                  << std::endl;
        return -1;
    }
    m_data->read_cb = cb;
    int result = s_gatt_client_interface->read_characteristic(
                     m_conn_id, ( m_data->handles[uuid] )[index], 0 );
    if( BT_STATUS_SUCCESS != result )
    {
        std::cerr << __func__ << ": Read FAILED with code " << result << std::endl;
        m_data->read_cb = nullptr;
        return -1;
    }
    m_data->available = false;
    // std::cout << "Read done. Waiting for response..." << std::endl;
    return 0;
}

int RivieraGattClient::Connection::SetMTU( unsigned int new_mtu )
{
    // TODO: Add back in once status code in once status code in
    //  configure_mtu_callback is observed to work as expected
    /*
    if (new_mtu == m_data->mtu) {
        // No need to set MTU to the same value
        return 0;
    }
    */
    int result = s_gatt_client_interface->configure_mtu(
                     m_conn_id, static_cast<int>( new_mtu ) );
    if( BT_STATUS_SUCCESS != result )
    {
        std::cerr << __func__ << ": SetMTU FAILED with code " << result
                  << std::endl;
        return -1;
    }
    m_data->available = false;
    return 0;
}

unsigned int RivieraGattClient::Connection::GetMTU()
{
    return m_data->mtu;
}
