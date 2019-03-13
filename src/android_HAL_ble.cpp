

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

#include "android_HAL_ble.hpp"

//#include <signal.h> 
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

#include <signal.h> // for raise()








// Androind hal hardware structs
namespace { // anonymous namespace to prevent pollution
    
    // Constants
    //static const int UUID_BYTES_LEN = 16;
    
    // Connection details
    bt_uuid_t s_client_uuid{};
    std::shared_ptr<bt_bdaddr_t> s_bda;
    std::atomic_int s_client_if(-1);
    std::atomic_int s_conn_id(-1);
    
    // State flags
    std::atomic_bool s_fluoride_on(false);          // Per process (must remain static global)
    std::atomic_bool s_client_registered(false);    // Per process
    std::atomic_bool s_found_device(false);         // Per connection (can be rolled into an object)
    std::atomic_bool s_client_connected(false);     // Per connection
    std::atomic_bool s_pending_request(false);    // Per connection
    std::shared_ptr<bt_bdaddr_t> s_conn_addr;                    // Per connection (needs to be thread safe)
 
    
    
    struct hw_device_t *pHWDevice;
    hw_module_t *pHwModule;
    bluetooth_device_t *pBTDevice;
    //std::shared_ptr<bluetooth_device_t> pBTDevice;
    //std::shared_ptr<const bt_interface_t> pBluetoothStack;
    std::shared_ptr<const bt_interface_t> pBluetoothStack;
    //std::shared_ptr<btvendor_interface_t> btVendorInterface;  // Not doing any vendor-base operations...

    // GATT
    std::shared_ptr<btgatt_interface_t> pGATTInterface;
    std::shared_ptr<const btgatt_client_interface_t> s_GATT_client_interface;
    std::shared_ptr<btgatt_db_element_t> s_gatt_uuid_db;
    std::atomic_int s_gatt_uuid_count(0);
    std::string s_read_response; // this should be made thread-safe...


    std::string uuid_stringer(const uint8_t* uuid) {
        if (!uuid) {
            std::cerr << __func__ << ": Empty uuid" << std::endl;
            return std::string("");
        }
        std::stringstream ss;
        for(int i = UUID_BYTES_LEN-1; i >= 0; --i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uuid[i]);
        }
        return ss.str();
    }
    
    
    std::string uuid_stringer(bt_uuid_t* uuid) {
        if (!uuid) {
            std::cerr << __func__ << ": Empty bt_uuid_t" << std::endl;
            return std::string("");
        }
        return uuid_stringer((uuid->uu));
    }


    


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
        static const std::string RUBEUS_NAME_HEADER(/*"RubeusBT"*/ "Peripheral_");

        bool name_match = false;
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
                std::size_t found = name.find(RUBEUS_NAME_HEADER);
                if (found != std::string::npos) {
                    name_match = true;
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
        if (name_match && manuf_id_match) {
            std::cout << ss.str() << std::endl;
            std::cout << "Found rubeus buds!" << std::endl;
            time_to_stop = true;
        }

        // Call back into class to stop scanning
        if (time_to_stop && s_GATT_client_interface) {
            s_GATT_client_interface->scan(false);
            //p_gattthis->Connect( s_client_if, bda );
            s_bda.reset(bda);
            s_found_device = true;
            //s_conn_addr.reset(bt_bdaddr_t); // TODO: THIS IS SO GRODY. SEPERATE CONNECTION FROM ADDRESS SOMEHOW
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
        s_client_connected = true;
    }


    void DisconnectClientCallback(int conn_id, int status, int client_if, bt_bdaddr_t* bda) {
        std::cout << "D_I_S_C_O_N_N_E_C_T_E_D, client_if:" << client_if << " conn_id: " << conn_id << "uuid:0x";
        for (int i = 0; i < UUID_BYTES_LEN; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bda->address[i]);
        }
        std::cout << std::endl;
        s_client_connected = false;
    }


    void search_complete_callback(int conn_id, int status) {
        std::cout << "Service search complete on conn_id " << conn_id << " with status 0x" << std::hex << status << std::endl;
        s_pending_request = false;
    }


    static const std::map<bt_gatt_db_attribute_type_t, std::string> GATT_TYPES = {
            {BTGATT_DB_PRIMARY_SERVICE, "primary service"},
            {BTGATT_DB_SECONDARY_SERVICE, "secondary service"},
            {BTGATT_DB_INCLUDED_SERVICE, "included service"},
            {BTGATT_DB_CHARACTERISTIC, "characteristic"},
            {BTGATT_DB_DESCRIPTOR, "descriptor"}      
        };


    void get_gatt_db_callback(int conn_id, btgatt_db_element_t* db, int count) {
        

        // Seems like it might be unreliable...
        s_gatt_uuid_db.reset(db);
        s_gatt_uuid_count = count;
        
        std::cout << "Gatt DB Callback from " << conn_id << ", got " << std::dec << count << " hits:" << std::endl;
        for (int i=0; i<count; ++i) {
            std::cout << std::dec << i << " **** ID: 0x" << std::hex << static_cast<int>(db[i].id) << std::endl;
            std::cout << "\t UUID: 0x" << uuid_stringer(&(db[i].uuid)) << std::endl;
            std::cout << "\t type: " << GATT_TYPES.at(db[i].type) << std::endl;
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
        s_pending_request = false;
    }


    /**
     * Find uuid in the db
     * @return: non-zero handle on success
     */
    uint16_t find_characteristic_handle_from_uuid(const uint8_t uuid[UUID_BYTES_LEN]) {
        if (!s_gatt_uuid_db || s_gatt_uuid_count<=0) {
            std::cerr << __func__ << ": GATT client db uninitialized" << std::endl;
            return 0;
        }
        /*if (sizeof(uuid)/sizeof(uuid[0]) != UUID_BYTES_LEN) {
            std::cerr << __func__ << ": UUID incorrect byte length" << std::endl;
            return 0;
        }*/
        
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
    }


    void write_characteristic_cb(int conn_id, int status, uint16_t handle)
    {
        std::cout << "Write on conn_id " << conn_id << ", handle " << std::hex << handle << std::dec << 
            " complete with status code " << status << std::endl;
        s_pending_request = false;
    }


    void read_characteristic_cb(int conn_id, int status, btgatt_read_params_t *p_data)
    {
        if (!p_data) {
            std::cerr << __func__ << ": no data recieved!" << std::endl;
            s_read_response.clear();
        } else {
            std::cout << "Read on conn_id " << conn_id << ", handle 0x" << std::hex << p_data->handle << std::dec << 
                " complete with status code " << status << std::endl;
            s_read_response = std::string(reinterpret_cast<char*>(p_data->value.value), p_data->value.len);
        }
        s_pending_request = false;
    }
    

    void execute_write_cb(int conn_id, int status) 
    {
        std::cout << "Write execute on " << conn_id << "competed with status " << status << std::endl;
        s_pending_request = false; 
    }


    void register_for_notification_cb(int conn_id, int registered, int status, uint16_t handle)
    {
        std::cout << "Conn id " << conn_id << ", handle " << std::hex << handle << std::hex << " notify register code " << registered << std::endl;
        s_pending_request = false; 
    }


    void notify_cb(int conn_id, btgatt_notify_params_t *p_data)
    {
        std::string content(reinterpret_cast<char*>(p_data->value), p_data->len);
        std::cout << "Got notification from " << conn_id << ": " << content;
    }

    void write_descriptor_cb(int conn_id, int status, uint16_t handle) 
    {
        std::cout << __func__ << std::endl;
        s_pending_request = false;
    }

    btgatt_client_callbacks_t sBTGATTClientCallbacks = {
        RegisterClientCallback,
        ScanResultCallback,
        ConnectClientCallback, // connect_callback
        DisconnectClientCallback, // disconnect_callback
        search_complete_callback,
        register_for_notification_cb,
        notify_cb,
        read_characteristic_cb,
        write_characteristic_cb,
        NULL, // read_descriptor_cb,
        write_descriptor_cb,
        execute_write_cb,
        NULL, // read_remote_rssi_callback
        NULL, //ListenCallback,
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


    btgatt_callbacks_t sBTGATTCallbacks = {
        sizeof( sBTGATTCallbacks ),
        &sBTGATTClientCallbacks,
        &sBTGATTServerCallbacks,
    };
    



static void AdapterStateChangeCb( bt_state_t state )
{
    //std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Fluoride Stack " << ((BT_STATE_ON == state) ? "enabled" : "disabled") << std::endl;
    s_fluoride_on = (BT_STATE_ON == state);
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
    for (int i = 0; i < UUID_BYTES_LEN; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bd_addr->address[i]);
        }
    std::cout << " : " << std::endl;
    for (int entry=0; entry<num_properties; ++entry) {
        std::cout << "\t";
        uint8_t* property_val = static_cast<uint8_t*>(properties[entry].val);
        for (int part=0; part<(properties[entry].len); ++part) {
            std::cout << static_cast<int>(property_val[part]);
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
    std::cout << "Fluoride Stack acl " << ((BT_ACL_STATE_CONNECTED == state) ? "connected" : "disconnected") << std::endl;
}

static void CbThreadEvent( bt_cb_thread_evt event )
{
    //std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Upper level JVM has been " << ((event == ASSOCIATE_JVM) ? "associated" : "disassociated") << std::endl;
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


static bt_callbacks_t sBluetoothCallbacks = {
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





} // end anonymous namespace



// Anonymous namespace relating to btproperty process
namespace {
    
    // Location of unix domain socket to talk with btproperty
    const char* BTPROP_UDS_FILE = "/data/misc/bluetooth/btprop";

    // Name of application
    const char* BTPROP_PROCESS = "btproperty";

    /** 
     * Removes socket associated with btproperty process.
     * @return 0 on success, non-zero on failure 
     */
    int btprop_remove_socket() {
        if (access(BTPROP_UDS_FILE, F_OK) != 0) {
            std::cout << "Socket " << BTPROP_UDS_FILE << " does not exist, no need to remove" << std::endl;
            return 0;
        }
        if (remove(BTPROP_UDS_FILE) != 0) {
            std::cout << "Could not remove socket " << BTPROP_UDS_FILE << std::endl;
            return 1;
        }
        return 0;
    }

    pid_t get_other_pid(std::string procname)
    {
        // The -s on pidoof means theres MAX 1 result, based off first hit
        // TODO: might be cleaner to do with sprintf or something
        std::string cmd("pidof -s ");
        cmd.append(procname);
        cmd.append(" 2>/dev/null"); // direct stderr INTO THE TRASH

        const size_t LEN = 8;
        char line[LEN];

        FILE* pipe = popen(cmd.c_str(), "r");
        fgets(line, LEN, pipe);
        pid_t pid = strtoul(line, nullptr, 10);
        pclose(pipe);

        return pid;
    }

    /**
     * Returns btproperty PID if it is running
     * @return btproperty PID if running, negative number if not
     */
    pid_t btprop_is_running() {
        return get_other_pid(BTPROP_PROCESS);
    }

    int btprop_launch_process() {
        std::cout << __func__ <<" NOT IMPLEMENTED YET!" << std::endl;
        raise(SIGABRT);
        // TODO: Start btproperty as a child process, I guess?
    }


}

    





//extern   hw_module_t HAL_MODULE_INFO_SYM; // WWWWHYYYY ?????!
int BTSetup() 
{

    //memset( s_client_uuid.uu, 0xDA, sizeof( s_client_uuid.uu ) );

    pid_t pid = btprop_is_running();
    if (!pid) {
        std::cout << "bt support process not running!" << std::endl;
        return 1;
    }
    
    int result;
    hw_module_t *pHwModule;
    
    result = hw_get_module(BT_STACK_MODULE_ID, (hw_module_t const **) &pHwModule);
    if (result != 0) {
        std::cout << "module could NOT be GOT" << std::endl;
        return 1;
    }    
    
    //pHwModule = &HAL_MODULE_INFO_SYM;

    result = pHwModule->methods->open(pHwModule, BT_STACK_MODULE_ID, &pHWDevice);
    if (result != 0) {
        std::cout << "BT Stack hw module got messed uppppp." << std::endl;
        return 1;
    }

    pBTDevice = reinterpret_cast<bluetooth_device_t*>(pHWDevice);
    //pBTDevice = (bluetooth_device_t*)(pHWDevice);

    pBluetoothStack = std::make_shared<bt_interface_t>(*pBTDevice->get_bluetooth_interface());
    //pBluetoothStack.reset(pBTDevice->get_bluetooth_interface());

    if( !pBluetoothStack )
    {
        BTShutdown();
        std::cout << "IT BLEW UPPPP" << std::endl;
        return 1;
    }
    std::cout << "BT has been set UP." << std::endl;
    
    
    // Do I need to reimplement this? or can I leave it?
    result = pBluetoothStack->init(&sBluetoothCallbacks); // if /usr/bin/btproperty is not running, this will exit your program!
    if (result != BT_STATUS_SUCCESS) {
        std::cout << "OH CRAP: " << result << std::endl;
        BTShutdown();
        std::cout << "IT BLEW UPPPP" << std::endl;
        return 1;
    }


    pBluetoothStack->enable(true);
    
    // I'm not using the vendor stuff right now so no need for this yet
    //auto ptr = pBluetoothStack->get_profile_interface( BT_PROFILE_VENDOR_ID );
    //btVendorInterface.reset( ( btvendor_interface_t* )ptr );
    
    // BT STACK NEEDS TO ENABLE BEFORE WE CAN CONTINUE
    // TODO: Make this based on the AdapterStateChangeCb firing
    //const int WAIT_TIME=3;
    std::cout << "Waitin for bt to set up before settin up GATT" << std::endl;
    //for (int i=1;i<=WAIT_TIME;++i) {
    //    sleep(1);
    //    std::cout << i << "..." << std::endl;
    //}
    while(!s_fluoride_on);
    std::cout << "DONE WAITIN" << std::endl;

    pGATTInterface.reset( (btgatt_interface_t*)pBluetoothStack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    //pGATTInterface = std::make_shared<btgatt_interface_t*>(pBluetoothStack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    if (!pGATTInterface) {
        std::cout << "GRABBIN THE GATT INTERFACE WENT SCREWBALLLZ" << std::endl;
        BTShutdown();
        return 1;
    }
    
    result = pGATTInterface->init(&sBTGATTCallbacks);
    if (result != BT_STATUS_SUCCESS) {
        std::cout << __LINE__ << " oh crap!" << std::endl;
    }
    
    s_GATT_client_interface.reset(pGATTInterface->client);

    // I dunno why uuid doesn't need to be filled out, but it works in CastleBluetooth...
    result = s_GATT_client_interface->register_client(&s_client_uuid);
    if (!s_GATT_client_interface || result != BT_STATUS_SUCCESS) {
        std::cout << __LINE__ << " oh crap! error: " << result << std::endl;
    } else {
    }
    std::cout << "registered client, waitin for acceptance..." << std::endl;
    while(!s_client_registered);
    std::cout << "CLIENT REGISTERED" << std::endl;
    return 0;
}



// TODO: Timeout doesn't work!
// TODO: Also ignoring exact match for now
// In fact, ignoring name too! HAHAHAHAHAHAHAHAHAHHAAAAAAA
int BTConnect(std::string name, bool exact_match, int timeout)
{
    s_GATT_client_interface->scan(true);
    std::cout << "started scanning..." << std::endl;
    time_t start = time(nullptr);
    while (/*((timeout >= 0) && (time(nullptr) > start + timeout)) ||*/ !s_found_device);
    if(!s_found_device) {
        s_GATT_client_interface->scan(false);
        std::cout << "Timed out while scanning!" << std::endl;
        return -1;
    }
    std::cout << "Found device!" << std::endl;
    s_GATT_client_interface->connect(s_client_if, s_bda.get(), true, 0);
    std::cout << "Attempting to connect..." << std::endl;
    while (!s_client_connected);


    s_pending_request = true;
    s_GATT_client_interface->search_service(s_client_if, nullptr);
    while (s_pending_request);

    std::cout << "Fetching gatt database..." << std::endl;
    s_pending_request = true;
    s_GATT_client_interface->get_gatt_db(s_conn_id);
    while(s_pending_request);

    return 0;
}


int BLEWriteCharacteristic(const uint8_t uuid[UUID_BYTES_LEN], std::string to_write)
{
    std::string uuid_str(uuid_stringer(uuid));
    std::cout << "searching for uuid " << uuid_str << std::endl;
    uint16_t handle = find_characteristic_handle_from_uuid(uuid);
    if (handle == 0) {
        std::cerr << __func__ << ": Couldn't find handle associated with uuid " << uuid_str << std::endl; 
        return -1;
    }
    std::cout << "Found handle 0x" << std::hex << handle << std::dec << " for uuid " << uuid_str << ", writing '" << to_write << "'..." << std::endl;

    int result = s_GATT_client_interface->write_characteristic(s_conn_id, handle, 1, to_write.size(), 0, const_cast<char*>(to_write.c_str()));
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Write FAILED with code " << result << std::endl;
        return -1;
    }
    s_pending_request = true;
    std::cout << "Write  done. Waiting for response..." << std::endl;
    while (s_pending_request);


    result = s_GATT_client_interface->execute_write(s_conn_id, 1);
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Write execute FAILED with code " << result << std::endl;
        return -1;
    }
    s_pending_request = true;
    std::cout << "Executing write. Waiting for response..." << std::endl;
    while (s_pending_request);

    return 0;
}

std::string BLEReadCharacteristic(const uint8_t uuid[16])
{
    uint16_t handle = find_characteristic_handle_from_uuid(uuid);
    if (handle == 0) {
        std::cerr << __func__ << ": Couldn't find handle associated with uuid" << std::endl; 
        return std::string();
    }
    int result = s_GATT_client_interface->read_characteristic(s_conn_id, handle, 0);
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Read FAILED with code " << result << std::endl;
        return std::string();
    }
    s_pending_request = true;
    std::cout << "Read done. Waiting for response..." << std::endl;
    while (s_pending_request);
    return s_read_response;
}


int BLENotifyRegister(const uint8_t uuid[16])
{
    uint16_t handle = find_characteristic_handle_from_uuid(uuid);
    if (handle == 0) {
        std::cerr << __func__ << ": Couldn't find handle associated with uuid" << std::endl; 
        return -1;
    }
    std::cout << "Attempting to register for notifications on client " << s_client_if << ", handle 0x" << std::hex << handle << std::dec << std::endl;
    int result = s_GATT_client_interface->register_for_notification(s_client_if, s_bda.get(), 0x11/*handle*/);
    //int result = s_GATT_client_interface->write_descriptor(s_conn_id, handle+1, 1, 1, 0, "1");
    if (BT_STATUS_SUCCESS != result) {
        std::cerr << __func__ << ": Register FAILED with code " << result << std::endl;
        return -1;
    }
    s_pending_request = true;
    std::cout << "Register for notification submitted. Waiting for response..." << std::endl;
    while (s_pending_request);
    return 0;
}


int BTShutdown()
{
    if (s_client_connected) {
        std::cout << "disconnecting client" << std::endl;
        s_GATT_client_interface->disconnect(s_client_if, s_bda.get(), s_conn_id);
        while(s_client_connected); // Wait for disconnect
    }
    
    if (s_client_registered) {
        std::cout << "unregistering client" << std::endl;
        s_GATT_client_interface->unregister_client(s_client_if);
        s_client_registered = false;
    }
    
    s_GATT_client_interface->unregister_client(s_client_if);
    
    if (pGATTInterface) {
        std::cout << "Cleaning up GATT object..." << std::endl;
        //pGATTInterface->cleanup(); // This segfaults
        //pGATTInterface.reset();    // This also sefgaults!
        //...but with them I get a bus error???
    }
    if (pBluetoothStack) {
        std::cout << "Cleaning up bluetooth stack object..." << std::endl;
        pBluetoothStack->disable();
        pBluetoothStack->cleanup();
        pBluetoothStack.reset();
    }

    pBTDevice->common.close( ( hw_device_t * ) & pBTDevice->common );
    pBTDevice = NULL;
    std::cout << "Shutdown complete" << std::endl;
    
    return 0;
}







/*

void FluorideBluetoothAdapter::CreateAdapters( const Callback<const BluetoothMsgs::BluetoothMsgIds&, std::shared_ptr<google::protobuf::Message>> &cb, const Callback<void>& readyCallback )
{
    m_logger.LogInfo( "CreateAdapters" );

    FluorideBluetoothDEVMAdapter::StackInitialize( m_pBluetoothStack );

    auto ptr = m_pBluetoothStack->get_profile_interface( BT_PROFILE_VENDOR_ID );
    m_btVendorInterface.reset( ( btvendor_interface_t* )ptr );

    m_logger.LogInfo( "Vendor interface: %p", ptr );
    m_pDEVM_Adapter = std::make_shared<FluorideBluetoothDEVMAdapter>( m_pBluetoothStack, m_btVendorInterface, m_frontdoor, m_task );
    m_ProfileAdapterList.push_back( m_pDEVM_Adapter );

    m_pA2DP_Adapter = std::make_shared<FluorideBluetoothA2DPAdapter>( m_pBluetoothStack, m_pAudioServer );
    m_ProfileAdapterList.push_back( m_pA2DP_Adapter );

    if( m_config )
    {
        auto config = m_config->ReadConfig();
        if( config.has_source_enabled() && config.source_enabled() )
        {
            m_logger.LogInfo( "CONFIG VALID - A2DP SOURCE ADAPTER ENABLED" );
            m_pA2DP_Source_Adapter = std::make_shared<FluorideBluetoothA2DPSourceAdapter>( m_pBluetoothStack, m_pAudioClient );
            m_ProfileAdapterList.push_back( m_pA2DP_Source_Adapter );
        }
        else
        {
            m_logger.LogInfo( "CONFIG VALID - A2DP SOURCE ADAPTER DISABLED" );
        }
    }
    else
    {
        m_logger.LogInfo( "NO CONFIG - A2DP SOURCE ADAPTER DISABLED" );
    }

    m_pAVRCP_Adapter = std::make_shared<FluorideBluetoothAVRCPAdapter>( m_pBluetoothStack );
    m_ProfileAdapterList.push_back( m_pAVRCP_Adapter );

    m_pGATT_Adapter = std::make_shared<FluorideBluetoothGATTAdapter>( m_pBluetoothStack, m_btVendorInterface );
    m_ProfileAdapterList.push_back( m_pGATT_Adapter );

    m_pSPP_Adapter = std::make_shared<FluorideBluetoothSPPAdapter>( m_pBluetoothStack );
    m_ProfileAdapterList.push_back( m_pSPP_Adapter );

    for( auto adapter : m_ProfileAdapterList )
    {
        adapter->Initialize( cb );
    }

    readyCallback();
}




void FluorideBluetoothGATTAdapter::Initialize( const BTDefines::SendEventToManagerCB &cb )
{
    m_SendToEventManagerCb = cb;
    m_pGATTInterface.reset( ( btgatt_interface_t* ) m_pBluetoothStack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    m_pGATTInterface->init( &sBTGATTCallbacks );
    m_pGATTServerInterface.reset( m_pGATTInterface->server );
    m_s_GATT_client_interface.reset( m_pGATTInterface->client );
    m_pCommandAdapter = std::make_shared<FluorideBluetoothCommandGATTAdapter>( m_task, m_pBluetoothStack, m_pGATTServerInterface, m_s_GATT_client_interface, m_btVendorInterface );
    p_gattthis = shared_from_this();
    m_pCommandAdapter->Initialize( cb );

    // NEW STUFFF
    memset( s_client_uuid.uu, 0xDA, sizeof( s_client_uuid.uu ) );
    //m_s_GATT_client_interface->register_client( &s_client_uuid );
    m_s_GATT_client_interface->scan( true );
}



const static int BT_PROPERTY_SHUTDOWN_WAIT_TIME_MS = 5000;
void FluorideBluetoothAdapter::Shutdown()
{
    m_logger.LogInfo( "Shutting Down" );
    //dont cleanup if btproperty is already gone.
    if( m_pBluetoothStack && ( m_childPID > 0 ) )
    {
        m_logger.LogInfo( "Shutdown - cleanup" );
        m_processKillTimer->SetTimeouts( BT_PROPERTY_SHUTDOWN_WAIT_TIME_MS, 0 );
        m_processKillTimer->Start( std::bind( &FluorideBluetoothAdapter::ProcessKill, shared_from_this() ) );
        m_pBluetoothStack->disable();
        m_pBluetoothStack->cleanup();
        m_pBluetoothStack.reset();
        m_logger.LogInfo( "Shutdown - cleanup complete" );
        ProcessKill();
    }
}







static btgatt_server_callbacks_t sBTGATTServerCallbacks =
{
    RegisterServerCallback,
    ConnectionCallback,
    ServiceAddedCallback,
    IncludedServiceAddedCallback,
    CharacteristicAddedCallback,
    DescriptorAddedCallback,
    ServiceStartedCallback,
    ServiceStoppedCallback,
    ServiceDeletedCallback,
    RequestReadCallback,
    RequestWriteCallback,
    RequestExecWriteCallback,
    ResponseConfirmationCallback,
    IndicationSentCallback,
    CongestionCallback,
    MtuChangedCallback
};
*/





