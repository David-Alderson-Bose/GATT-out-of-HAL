
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
} 



// Anonymous namespace for data storage
namespace {
    // Hold list of connections
    std::unordered_map<std::string, RivieraGattClient::ConnectionPtr> s_connections;
    



    // Gatt client interface and tracking
    std::shared_ptr<const btgatt_client_interface_t> s_gatt_client_interface;
    std::atomic_bool s_client_registered(false);
    bt_uuid_t s_client_uuid{};
    std::atomic_bool s_scanning(false);

    // Connecting is a mess of callbacks and needs some of its own data
    std::atomic_bool s_connecting(false);
    std::atomic_int s_conn_id(-1);
    std::shared_ptr<bt_bdaddr_t> s_bda; 

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
                for (auto& kv : s_connections) {
                    std::size_t found = name.find(kv.first);
                    if (found != std::string::npos || kv.second == nullptr) {
                        matched_name == kv.first;
                        break;
                    }
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
        //
        //if (!name.empty()) {
        //    std::cout << ss.str() << std::endl;
        //}

        // PACK IT IN, BOYS
        if (!matched_name.empty() && manuf_id_match) {
            std::cout << ss.str() << std::endl;
            std::cout << "Found rubeus buds!" << std::endl;
            time_to_stop = true;
        }

        // Call back into class to stop scanning
        if (time_to_stop && s_gatt_client_interface) {
            s_gatt_client_interface->scan(false);
            s_bda.reset(bda);
            s_scanning = false;
        }
        ++count;
    }
    

    void ConnectClientCallback(int conn_id, int status, int client_if, bt_bdaddr_t* bda) {
        std::cout << "CONNECTED, client_if:" << client_if << " conn_id: " << conn_id << " address:0x";
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
        //s_pending_request = false;
    }


    


    void get_gatt_db_callback(int conn_id, btgatt_db_element_t* db, int count) {
        // Seems like it might be unreliable...
        //s_gatt_uuid_db.reset(db);
        //s_gatt_uuid_count = count;
        
        std::cout << "Gatt DB Callback from conn_id " << conn_id << ", got " << std::dec << count << " hits:" << std::endl;
        for (int i=0; i<count; ++i) {
            std::cout << std::dec << i << " **** ID: 0x" << std::hex << static_cast<int>(db[i].id) << std::endl;
            std::cout << "\t UUID: 0x" << RivieraBT::StringifyUUID(&(db[i].uuid)) << std::endl;
            std::cout << "\t type: " << GATT_TYPES_MAP.at(db[i].type) << std::endl;
            if (db[i].type == BTGATT_DB_PRIMARY_SERVICE || db[i].type == BTGATT_DB_SECONDARY_SERVICE) {
                std::cout << "\t start_handle: 0x" << std::hex << static_cast<int>(db[i].start_handle) << std::endl;
                std::cout << "\t end_handle: 0x" << std::hex << static_cast<int>(db[i].end_handle) << std::endl;
            } else if (db[i].type == BTGATT_DB_CHARACTERISTIC) {
                std::cout << "\t attribute handle: 0x" << std::hex << static_cast<int>(db[i].attribute_handle) << std::endl;
                std::cout << "\t properties: 0x" << std::hex << static_cast<int>(db[i].properties) << std::endl;
            } else { 
                std::cout << "\t attribute handle: 0x" << std::hex << static_cast<int>(db[i].attribute_handle) << std::endl;
            }
        }
        //s_pending_request = false;
    }


    /**
     * Find uuid in the db
     * @return: non-zero handle on success
     */
    uint16_t find_characteristic_handle_from_uuid(const uint8_t uuid[UUID_BYTES_LEN]) {
        /*
        if (!s_gatt_uuid_db || s_gatt_uuid_count<=0 false) {
            std::cerr << __func__ << ": GATT client db uninitialized" << std::endl;
            return 0;
        }
        if (sizeof(uuid)/sizeof(uuid[0]) != UUID_BYTES_LEN) {
            std::cerr << __func__ << ": UUID incorrect byte length" << std::endl;
            return 0;
        }
        
        // Search
        uint16_t return_handle = 0; 
        for (int entry=0; entry<s_gatt_uuid_count; ++entry) {
            if (s_gatt_uuid_db.get()[entry].type != BTGATT_DB_CHARACTERISTIC) {
                continue;
            }
            uint8_t* device_uuid = s_gatt_uuid_db.get()[entry].uuid.uu;
            if (0 == memcmp(uuid, device_uuid, UUID_BYTES_LEN)) {
                return_handle = s_gatt_uuid_db.get()[entry].attribute_handle;
                break;
            }
        }
        return return_handle;
        */
       return 0;
    }


    void write_characteristic_cb(int conn_id, int status, uint16_t handle)
    {
        std::cout << "Write on conn_id " << conn_id << ", handle 0x" << std::hex << handle << std::dec << 
            " complete with status code " << status << std::endl;
        //s_pending_request = false;
    }


    void read_characteristic_cb(int conn_id, int status, btgatt_read_params_t *p_data)
    {
        if (!p_data) {
            std::cerr << __func__ << ": no data recieved!" << std::endl;
            //s_read_response.clear();
        } else {
            std::cout << "Read on conn_id " << conn_id << ", handle 0x" << std::hex << p_data->handle << std::dec << 
                " complete with status code " << status << std::endl;
            //s_read_response = std::string(reinterpret_cast<char*>(p_data->value.value), p_data->value.len);
        }
        //s_pending_request = false;
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
        NULL, // ConfigureMtuCallback, // configure_mtu_callback
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
    btgatt_callbacks_t sBTGATTCallbacks = {
        sizeof( sBTGATTCallbacks ),
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
               // TODO: Error message?
               return -1;
            }
        }
    
        // To the registrar!
        s_gatt_client_interface.reset(RivieraBT::GetGatt()->client);
        int registered = s_gatt_client_interface->register_client(&s_client_uuid);
        if (!s_gatt_client_interface || registered != BT_STATUS_SUCCESS) {
            // TODO: Error message?
            return -1;
        }

        while (!s_client_registered); // Kill some time
        return 0;
    }


    int clear_connect_data() {
        if (s_connecting != false) {
            return -1;
        }
        s_conn_id = -1;
        s_bda.reset();
        return 0;
    }
}



RivieraGattClient::ConnectionPtr Connect(std::string name, bool exact_match, int timeout)
{
    if (gatt_setup() != 0) {
        return nullptr;
    }

    // Do something else if multiples of same name are allowed
    if (s_connections.count(name) != 0 && s_connections[name] != nullptr) {
        std::cerr << "Already registered " << name << std::endl;
    } 

    // Load name to scan for
    s_connections[name] = nullptr;
    
    // Wait in case scanning already
    while (s_scanning);

    // Start scan and wait - scan will initiate connection
    s_scanning = true;
    s_gatt_client_interface->scan(true);
    while (s_scanning);

    // Connect if possible
    if (!s_bda) {
        std::cerr << "No record of address for " << name << std::endl;
        return nullptr;
    }
    s_conn_id = -1;
    s_gatt_client_interface->connect(s_client_if, s_bda.get(), true, 0);
    while (s_conn_id < 0);

    s_connections[name] = std::make_shared<RivieraGattClient::Connection>(s_conn_id, s_bda.get());
    clear_connect_data();
    return s_connections[name];
}


RivieraGattClient::Connection::Connection(int conn_id, bt_bdaddr_t* bda) 
    : m_conn_id(conn_id)
    , m_bda(bda)
{}


int RivieraGattClient::Connection::WriteCharacteristic(const uint8_t uuid[UUID_BYTES_LEN], std::string to_write)
{
    return 0;
}


std::string RivieraGattClient::Connection::ReadCharacteristic(RivieraBT::UUID uuid)
{
    return std::string();
}


int RivieraGattClient::Connection::ReadCharacteristic(RivieraBT::UUID uuid, ReadCallback cb)
{
    return 0;
}
