
// TODO: I may not need all of these
#include <unordered_map>
#include <memory> // in case I want to use shared_ptr
#include <iostream> // for debug print
#include <iomanip> // for extra debug print options
#include <atomic>
#include <sstream>
#include <map>
#include <algorithm> // for equal
#include <iterator>

#include <string.h> // for strerror()
#include <errno.h> // for errno
#include <unistd.h> // for access()
#include <time.h> // for time()


#include <riviera_bt.hpp>
#include <riviera_gatt_client.hpp>



// Anonymous namespace for constants
namespace {
    const std::map<bt_gatt_db_attribute_type_t, std::string> GATT_TYPES_MAP = {
        {BTGATT_DB_PRIMARY_SERVICE, "primary service"},
        {BTGATT_DB_SECONDARY_SERVICE, "secondary service"},
        {BTGATT_DB_INCLUDED_SERVICE, "included service"},
        {BTGATT_DB_CHARACTERISTIC, "characteristic"},
        {BTGATT_DB_DESCRIPTOR, "descriptor"}      
    };

    const unsigned int DEFAULT_MTU = 23; // Industry standards
} 


struct ConnectionData {
        std::atomic_bool available;
        std::shared_ptr<btgatt_db_element_t> handles_db;
        int handles_count;
        RivieraGattClient::ConnectionPtr connection;
        RivieraGattClient::ReadCallback read_cb;
        std::map<RivieraBT::UUID, int> handles;
        unsigned int mtu;
        ConnectionData() : available(false), handles_db(nullptr), handles_count(-1), 
                           connection(nullptr), read_cb(nullptr), mtu(DEFAULT_MTU) {}
    };

// Anonymous namespace for data storage
namespace {

    // Hold list of connections
    std::unordered_map<int, ConnectionData> s_connections;

    // Gatt client interface and tracking
    std::shared_ptr<const btgatt_client_interface_t> s_gatt_client_interface;
    std::atomic_bool s_client_registered(false);
    bt_uuid_t s_client_uuid{};
    std::atomic_bool s_scanning(false);

    // Connecting is a mess of callbacks and needs some of its own data
    std::atomic_bool s_connecting(false);
    std::atomic_int s_conn_id(-1);
    //std::shared_ptr<bt_bdaddr_t> s_bda;
    bt_bdaddr_t* s_bda;
    std::string  s_name_to_scan;

    // Gatt client support 
    std::atomic_int s_client_if(-1);
}


// Anonymous namespace for GATT callbacks
namespace {
    
    // Callback that triggers once client is registered
    void RegisterClientCallback(int status, int client_if, bt_uuid_t *app_uuid) {
        std::cout << "Registered! uuid:0x";
        for( int i = 0; i < 16; i++ ) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(app_uuid->uu[i]);
        }
        std::cout << " client_if:" << client_if << std::endl << std::flush;
        s_client_if = client_if;
        s_client_registered = true;
    }


    // Callback when scan results are found
    void ScanResultCallback(bt_bdaddr_t* bda, int rssi, uint8_t* adv_data) {
        static const int ADV_DATA_MAX_LEN = 32;
        static const uint8_t COMPLETE_LOCAL_NAME_FLAG = 0x09;

        // Search specific stuff
        static const uint8_t ON_SEMI_ID[] {0x62, 0x03, 0x03, '\0'};

        std::string matched_name;
        bool manuf_id_match = /*false*/ true;

        static int count = 1;
        bool time_to_stop = false;
        std::string name;
        uint8_t manuf_id[] {0, 0, 0, '\0'};
        int loc = 0;

        // Accumulate info in a stringstream
        std::stringstream ss;
        ss << std::dec << count << ". ";

        // Parse address
        for (int field = 0; field < 6; ++field) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bda->address[field]);
            if (field < 5) {
                ss << ":";
            }
        }

        // Read advertising packet
        while (loc < ADV_DATA_MAX_LEN) {
            std::size_t len = adv_data[loc];
            if (len == 0 || len + loc > ADV_DATA_MAX_LEN) {
                break;
            }
            ss << std::dec << " | Len: " << len;
            uint8_t type = adv_data[loc + 1];
            ss << " Type: " << std::hex << "0x" << static_cast<int>( type );

            // check if type is shortened local name or complete local name
            if(type == 0x08 || type == 0x09) {
                name.assign(reinterpret_cast<char*>(adv_data) + loc + 2, len - 1);
                ss << " Name: " << name;
                std::size_t found = name.find(s_name_to_scan);;
                if (found != std::string::npos) {
                    matched_name = s_name_to_scan;
                }
            }
            else if (type == 0xFF) {   // manufacturer ID
                ss << " Manuf. ID: 0x";
                for (int i = 0; i < 3; i++) {
                    manuf_id[i] = adv_data[loc + 2 + i];
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(manuf_id[i]);
                }
                int match = strcmp(reinterpret_cast<const char*>(ON_SEMI_ID), reinterpret_cast<char*>(manuf_id));
                if (match == 0) {
                    manuf_id_match = true;
                }
            } else {
                ss << std::hex << " Data: 0x";
                for (int i = 1; i < len; ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(adv_data[loc + i]);
                }
            }
            loc += ( len + 1 );
        }

        // Only print if we got a name
        //if (!name.empty()) {
        //    std::cout << ss.str() << std::endl;
        //}

        // PACK IT IN, BOYS
        if (!matched_name.empty() && manuf_id_match) {
            std::cout << ss.str() << std::endl;
            std::cout << "Found device with name '" << s_name_to_scan << "'!" << std::endl;
            time_to_stop = true;
        }

        // Call back into class to stop scanning
        if (time_to_stop && s_gatt_client_interface) {
            s_gatt_client_interface->scan(false);
            //s_bda.reset(bda);
            s_bda = bda;
            s_scanning = false;
        }
        ++count;
    }
    

    void ConnectClientCallback(int conn_id, int status, int client_if, bt_bdaddr_t* bda) {
        std::cout << "CONNECTED, client_if:" << client_if << " conn_id:" << conn_id << " address:0x";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bda->address[i]);
        }
        std::cout << std::endl;
        s_conn_id = conn_id;
        s_connecting = false;
    }


    void DisconnectClientCallback(int conn_id, int status, int client_if, bt_bdaddr_t* bda) {
        std::cout << "D_I_S_C_O_N_N_E_C_T_E_D, client_if:" << client_if << ", conn_id:" << conn_id << ", uuid:0x";
        for (int i = 0; i < UUID_BYTES_LEN; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bda->address[i]);
        }
        std::cout << std::endl;
    }


    void search_complete_callback(int conn_id, int status) {
        std::cout << "Service search complete on conn_id " << conn_id << " with status " << std::hex << status << std::endl;
        s_scanning = false;
    }


    void get_gatt_db_callback(int conn_id, btgatt_db_element_t* db, int count) {
        if (s_connections.count(conn_id) > 0) {
            s_connections[conn_id].handles_db.reset(db);
            s_connections[conn_id].handles_count = count;
            s_connections[conn_id].available = true;
        }
    }




    void write_characteristic_cb(int conn_id, int status, uint16_t handle)
    {
        if (s_connections.count(conn_id) == 0 || !s_connections[conn_id].connection) {
            std::cerr << __func__ << ": got bad write callback on unclaimed conn_id " << conn_id;
            return;
        }
        std::cout << "Write on connection  '" << s_connections[conn_id].connection->GetName() << "', handle 0x" << 
            std::hex << handle << std::dec << " complete with status code " << status << std::endl;
        s_connections[conn_id].available = true;
    }


    void read_characteristic_cb(int conn_id, int status, btgatt_read_params_t *p_data)
    {
        if (s_connections.count(conn_id) == 0 || !s_connections[conn_id].connection) {
            std::cerr << __func__ << ": got bad write callback on unclaimed conn_id " << conn_id;
            return;
        }
        
        if (!p_data) {
            std::cerr << __func__ << ": no data recieved!" << std::endl;
        } else {
            if (s_connections[conn_id].read_cb) {
                s_connections[conn_id].read_cb(reinterpret_cast<char*>(p_data->value.value), p_data->value.len);
                s_connections[conn_id].read_cb = nullptr; // clear callback
            } else {
                std::cout << __func__ << ": recieved " << 
                    std::string(reinterpret_cast<char*>(p_data->value.value), p_data->value.len) << std::endl;
            }
        }
        s_connections[conn_id].available = true;
    }


    void configure_mtu_callback(int conn_id, int status, int mtu)
    {
        if (s_connections.count(conn_id) == 0 || !s_connections[conn_id].connection) {
            std::cerr << __func__ << ": got bad mtu callback on unclaimed conn_id " << conn_id;
            return;
        }
        //s_connections[conn_id].mtu = mtu; // TODO: add back in once status code is observed to work as expected
        std::cout << __func__ << ": MTU for connection ID " << conn_id << " negotiated to " << mtu << 
            " with status code " << status << std::endl;
        s_connections[conn_id].available = true;
    }


    btgatt_client_callbacks_t sBTGATTClientCallbacks = {
        RegisterClientCallback,
        ScanResultCallback,
        ConnectClientCallback, // connect_callback
        DisconnectClientCallback, // disconnect_callback
        search_complete_callback,
        NULL, //register_for_notification_cb,
        NULL, //notify_cb,
        read_characteristic_cb,
        write_characteristic_cb,
        NULL, // read_descriptor_cb,
        NULL, // write_descriptor_cb,
        NULL, // execute_write_cb,
        NULL, // read_remote_rssi_callback
        NULL, // ListenCallback,
        configure_mtu_callback,
        NULL, // scan_filter_cfg_callback
        NULL, // scan_filter_param_callback
        NULL, // scan_filter_status_callback
        NULL, // multi_adv_enable_callback
        NULL, // multi_adv_update_callback
        NULL, // multi_adv_data_callback
        NULL, // multi_adv_disable_callback
        NULL, // congestion_callback
        NULL, // batchscan_cfg_storage_callback
        NULL, // batchscan_enable_disable_callback
        NULL, // batchscan_reports_callback
        NULL, // batchscan_threshold_callback
        NULL, // track_adv_event_callback
        NULL, // scan_parameter_setup_completed_callback
        get_gatt_db_callback,
        NULL, // services_removed_callback
        NULL, // services_added_callback
    };
}


// Anonymous namespace for gatt server stuff
namespace {
    static btgatt_server_callbacks_t sBTGATTServerCallbacks = {
        NULL, //RegisterServerCallback,
        NULL, //ConnectionCallback,
        NULL, //ServiceAddedCallback,
        NULL, //IncludedServiceAddedCallback,
        NULL, //CharacteristicAddedCallback,
        NULL, //DescriptorAddedCallback,
        NULL, //ServiceStartedCallback,
        NULL, //ServiceStoppedCallback,
        NULL, //ServiceDeletedCallback,
        NULL, //RequestReadCallback,
        NULL, //RequestWriteCallback,
        NULL, //RequestExecWriteCallback,
        NULL, //ResponseConfirmationCallback,
        NULL, //IndicationSentCallback,
        NULL, //CongestionCallback,
        NULL, //MtuChangedCallback
    };
}


// Anonymous namespace (a cute lil one) for setup of the GATT device
namespace {
    btgatt_callbacks_t s_gatt_callbacks = {
        sizeof( s_gatt_callbacks ),
        &sBTGATTClientCallbacks,
        &sBTGATTServerCallbacks,
    };
}


// Anonymous namespace for static functions
namespace {
    int gatt_setup() {
        if (s_gatt_client_interface) {
            // Already good to go
            return 0;
        }
        
        // We might need to start from scratch...
        if (!RivieraBT::isSetup()) {
            if (RivieraBT::Setup() != 0) {
               std::cerr << __func__ << ": Could not set up riviera Bluetooth!" << std::endl;
               return -1;
            }
        }

        // Initialize GATT portion of BT/BLE stack
        if (BT_STATUS_SUCCESS != RivieraBT::GetGatt()->init(&s_gatt_callbacks)) {
            std::cerr << __func__ << ": Could not register gatt client!" << std::endl;
            return -1;
        }
    
        // To the registrar!
        s_gatt_client_interface.reset(RivieraBT::GetGatt()->client);
        int registered = s_gatt_client_interface->register_client(&s_client_uuid);
        if (!s_gatt_client_interface || registered != BT_STATUS_SUCCESS) {
            std::cerr << __func__ << ": Could not register gatt client!" << std::endl;
            return -1;
        }

        while (!s_client_registered); // Kill some time
        std::cout << "GATT setup complete" << std::endl;
        return 0;
    }


    int clear_connect_data() {
        if (s_connecting != false) {
            return -1;
        }
        s_conn_id = -1;
        //s_bda.reset(); // This causes a segfault!
        s_bda = nullptr;
        s_name_to_scan.clear();
        return 0;
    }
}


RivieraGattClient::ConnectionPtr RivieraGattClient::Connect(std::string name, bool exact_match, int timeout)
{
    if (gatt_setup() != 0) {
        std::cerr << __func__ << ": Gatt setup failed!" << std::endl;
        return nullptr;
    }

    // Do something else if multiples of same name are allowed
    for (auto& kv: s_connections) {
        if (kv.second.connection && kv.second.connection->GetName() == name) {
            std::cerr << "Already registered " << name << std::endl;
            return nullptr;
        }
    } 
    
    // Wait in case scanning already
    std::cout << "Checking for ongoing scan..." << std::endl;
    while (s_scanning);

    // Start scan and wait - scan will initiate connection
    std::cout << "Initiating scan for " << name << "..." << std::endl;
    s_name_to_scan = name; 
    s_scanning = true;
    s_gatt_client_interface->scan(true);
    while (s_scanning);

    // Connect if possible
    if (!s_bda) {
        std::cerr << "No record of address for " << name << std::endl;
        return nullptr;
    }
    std::cout << "Attempting to connect to " << name << "..." << std::endl;
    s_conn_id = -1;
    s_gatt_client_interface->connect(s_client_if, /*s_bda.get()*/ s_bda, true, 0);
    while (s_conn_id < 0);
    std::cout << "Finalizing connection..." << std::endl;

    s_connections[s_conn_id]; // create spot
    Connection* temp = new RivieraGattClient::Connection(name, s_conn_id, /*s_bda.get()*/ s_bda, &(s_connections[s_conn_id]));
    s_connections[s_conn_id].connection.reset(temp);
    s_connections[s_conn_id].available = true;
    s_connecting = false;
    s_scanning = true;
    s_gatt_client_interface->search_service(s_client_if, nullptr); // Need to do this before get_gatt_db will work
    while(s_scanning);
    s_connections[s_conn_id].connection->fetch_services();
    //clear_connect_data();
    return s_connections[s_conn_id].connection;
}


int RivieraGattClient::Connection::GetConnectionID() {
    return m_conn_id; 
}


std::string RivieraGattClient::Connection::GetName() {
    return m_name;
}

 
RivieraGattClient::Connection::Connection(std::string name, int conn_id, bt_bdaddr_t* bda, ConnectionData* data)
    : m_name(name)
    , m_conn_id(conn_id)
    , m_bda(bda)
{
    m_data.reset(data);
    std::cout << "Created connection " << m_name << " with conn_id " << std::dec << m_conn_id << std::endl;
}


void RivieraGattClient::Connection::PrintHandles() {
    for (auto& kv: m_data->handles) {
        std::cout << RivieraBT::StringifyUUID(kv.first) << " : " << kv.second << std::endl; 
    }
}


void RivieraGattClient::Connection::fill_handle_map() 
{
    if (!m_data->handles_db || m_data->handles_count <= 0) {
        std::cerr << __func__ << ": no handdles found" << std::endl;
        return;
    }
    for (int entry=0; entry<(m_data->handles_count); ++entry) {
        if (m_data->handles_db.get()[entry].type != BTGATT_DB_CHARACTERISTIC) {
            continue;
        }
        std::cout << "Adding entry for " << m_data->handles_db.get()[entry].attribute_handle << std::endl;
        RivieraBT::UUID uuid;
        memcpy(uuid.data(), m_data->handles_db.get()[entry].uuid.uu, UUID_BYTES_LEN);
        m_data->handles[uuid] = m_data->handles_db.get()[entry].attribute_handle;
    }
    std::cout << "Filled " << m_data->handles.size() << " entries in the handle map" << std::endl;
}



void RivieraGattClient::Connection::fetch_services(void) 
{
    std::cout << "Searching for services on " << m_name << " conn_id " << std::dec << m_conn_id << std::endl;
    //m_data->available = false;
    //s_gatt_client_interface->search_service(s_client_if, nullptr); // Need to do this before get_gatt_db will work
    //std::cout << std::dec << __FILE__ << "/" <<  __func__ << __LINE__ << " TRACKING!!!" << std::endl;
    //while (!m_data->available);
    //std::cout << std::dec << __FILE__ << "/" <<  __func__ << __LINE__ << " TRACKING!!!" << std::endl;
    
    std::cout << "Fetching gatt database..." << std::endl;
    m_data->available = false;
    s_gatt_client_interface->get_gatt_db(s_conn_id); // Need to do search_services before this will work
    std::cout << std::dec << __FILE__ << "/" <<  __func__ << __LINE__ << " TRACKING!!!" << std::endl;
    while(!m_data->available);
    std::cout << std::dec << __FILE__ << "/" <<  __func__ << __LINE__ << " TRACKING!!!" << std::endl;

    fill_handle_map();
    std::cout << "Finished attempt to fill handle map" << std::endl;
    PrintHandles();
}

/**
 * Find uuid in the db
 * @return: non-zero handle on success
 */
uint16_t RivieraGattClient::Connection::find_characteristic_handle_from_uuid(RivieraBT::UUID uuid) {
    std::cout << __func__ << ": 1" << std::endl;
    if (!m_data->handles_db || m_data->handles_count<=0) {
        std::cerr << __func__ << ": GATT client db uninitialized" << std::endl;
        return 0;
    }
    
    // Search
    uint16_t return_handle = 0; 
    for (int entry=0; entry<m_data->handles_count; ++entry) {
        std::cout << "entry " << entry << std::endl;
        if (m_data->handles_db.get()[entry].type != BTGATT_DB_CHARACTERISTIC) {
            continue;
        }
        uint8_t* device_uuid = m_data->handles_db.get()[entry].uuid.uu;
        if (0 == memcmp(uuid.data(), device_uuid, UUID_BYTES_LEN)) {
            return_handle = m_data->handles_db.get()[entry].attribute_handle;
            break;
        }
    }
    return return_handle;
}



int RivieraGattClient::Connection::WriteCharacteristic(RivieraBT::UUID uuid, std::string to_write)
{
    if (!m_data->available) {
        std::cerr << "Connection busy" << std::endl;
        return -1;
    }
    
    if (m_data->handles.empty()) {
       fetch_services();
    }
    
    std::string uuid_str(RivieraBT::StringifyUUID(uuid));
    if (m_data->handles.count(uuid) == 0) {
        std::cerr << __func__ << "UUID not available in connection" << std::endl;
        return -1;
    }
    
    uint16_t handle = m_data->handles[uuid];
    int result = s_gatt_client_interface->write_characteristic(m_conn_id, handle, 1, to_write.size(), 0, const_cast<char*>(to_write.c_str()));
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Write FAILED with code " << result << std::endl;
        return -1;
    }
    
    m_data->available = false;
    std::cout << "Write  done. Waiting for response..." << std::endl;

    return 0;
}


std::string RivieraGattClient::Connection::ReadCharacteristic(RivieraBT::UUID uuid)
{
    return std::string();
}


int RivieraGattClient::Connection::ReadCharacteristic(RivieraBT::UUID uuid, ReadCallback cb)
{
    // Removing to see if we can get multiple consecutive reads goin
    //if (!m_data->available) {
    //    std::cerr << "Connection busy" << std::endl;
    //    return -1;
    //}
    
    // TODO: This is a hack, add a timeout or something
    while (!m_data->available);
    
    if (m_data->handles.empty()) {
       fetch_services();
    }
    
    if (m_data->handles.count(uuid) == 0 || m_data->handles[uuid] <= 0) {
        std::cerr << __func__ << ": Couldn't find handle associated with uuid" << std::endl; 
        return -1;
    }
    m_data->read_cb = cb;
    int result = s_gatt_client_interface->read_characteristic(m_conn_id, m_data->handles[uuid], 0);
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Read FAILED with code " << result << std::endl;
        m_data->read_cb = nullptr;
        return -1;
    }
    m_data->available = false;
    //std::cout << "Read done. Waiting for response..." << std::endl;
    return 0;
}


int RivieraGattClient::Connection::SetMTU(unsigned int new_mtu)
{
    // TODO: Add back in once status code in once status code in 
    //  configure_mtu_callback is observed to work as expected
    /*
    if (new_mtu == m_data->mtu) {
        // No need to set MTU to the same value
        return 0;
    }
    */
    int result = s_gatt_client_interface->configure_mtu(m_conn_id, static_cast<int>(new_mtu));
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": SetMTU FAILED with code " << result << std::endl;
        return -1;
    }
    m_data->available = false;
    return 0;
}


unsigned int RivieraGattClient::Connection::GetMTU() {
    return m_data->mtu;
}
