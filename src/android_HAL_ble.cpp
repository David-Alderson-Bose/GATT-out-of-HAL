

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
#include <string.h> // for strerror()
#include <errno.h> // for errno
#include <unistd.h> // for access()

#include <signal.h> // for raise()








// Androind hal hardware structs
namespace { // anonymous namespace to prevent pollution
    
    bt_uuid_t s_client_uuid{};
    int s_client_if( -1 );
    bool client_registered = false;
    
    struct hw_device_t *pHWDevice;
    hw_module_t *pHwModule;
    bluetooth_device_t *pBTDevice;
    std::shared_ptr<bt_interface_t> pBluetoothStack;
    std::shared_ptr<btvendor_interface_t> btVendorInterface;



    // Callback that triggers once client is registered
    static void RegisterClientCallback(int status, int client_if, bt_uuid_t *app_uuid) {
        std::cout << "Registered! uuid:0x";
        for( int i = 0; i < 16; i++ ) {
            std::cout << std::hex << std::setw( 2 ) << std::setfill( '0' ) << static_cast<int>( app_uuid->uu[i] );
        }
        std::cout << " client_if:" << client_if << std::endl;
        s_client_if = client_if;
        client_registered = true;
    }






    std::shared_ptr<btgatt_interface_t> pGATTInterface;
    //std::shared_ptr<const btgatt_server_interface_t> m_pGATTServerInterface;
    std::shared_ptr<const btgatt_client_interface_t> pGATTClientInterface;
    
    
    
    btgatt_client_callbacks_t sBTGATTClientCallbacks = {
        RegisterClientCallback,
        NULL, //ScanResultCallback,
        NULL, //ConnectClientCallback, // connect_callback
        NULL, //DisconnectClientCallback, // disconnect_callback
        NULL, // search_complete_callback
        NULL, // register_for_notification_callback
        NULL, // notify_callback
        NULL, // read_characteristic_callback
        NULL, // write_characteristic_callback
        NULL, // read_descriptor_callback
        NULL, // write_descriptor_callback
        NULL, // execute_write_callback
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
        NULL, // get_gatt_db_callback
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
    std::cout << __func__ << ":" << __LINE__ << std::endl;
    std::cout << "Fluoride Stack " << ((BT_STATE_ON == state) ? "enabled" : "disabled") << std::endl;
}

static void AdapterPropertiesCb( bt_status_t status, int num_properties, bt_property_t *properties )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void RemoteDevicePropertiesCb( bt_status_t status, bt_bdaddr_t *bd_addr, int num_properties, bt_property_t *properties )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
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
    std::cout << __func__ << ":" << __LINE__ << std::endl;
}

static void CbThreadEvent( bt_cb_thread_evt event )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
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

    





extern   hw_module_t HAL_MODULE_INFO_SYM; // WWWWHYYYY ?????!
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
    /*
    result = hw_get_module (BT_STACK_MODULE_ID, (hw_module_t const **) &pHwModule);
    if (result != 0) {
        std::cout << "module could NOT be GOT" << std::endl;
        return 1;
    } */   
    
    pHwModule = &HAL_MODULE_INFO_SYM;

    result = pHwModule->methods->open(pHwModule, BT_STACK_MODULE_ID, &pHWDevice);
    if (result != 0) {
        std::cout << "BT Stack hw module got messed uppppp." << std::endl;
        return 1;
    }

    pBTDevice = reinterpret_cast<bluetooth_device_t*>(pHWDevice);
    pBluetoothStack = std::make_shared<bt_interface_t>( *pBTDevice->get_bluetooth_interface() );

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
    const int WAIT_TIME=3;
    std::cout << "Waitin for " << WAIT_TIME << " secs before settin up GATT" << std::endl;
    for (int i=1;i<=WAIT_TIME;++i) {
        sleep(1);
        std::cout << i << "..." << std::endl;
    }
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
    
    pGATTClientInterface.reset(pGATTInterface->client);

    // I dunno why uuid doesn't need to be filled out, but it works in CastleBluetooth...
    result = pGATTClientInterface->register_client(&s_client_uuid);
    if (!pGATTClientInterface || result != BT_STATUS_SUCCESS) {
        std::cout << __LINE__ << " oh crap! error: " << result << std::endl;
    } else {
    }
    std::cout << "registered client, waitin for acceptance" << std::endl;
    return 0;
}


int BTShutdown()
{
    if (client_registered) {
        std::cout << "unregistering client" << std::endl;
        pGATTClientInterface->unregister_client(s_client_if);
        client_registered = false;
    }
    
   //pGATTClientInterface->unregister_client(s_client_if);
    
    if (pGATTInterface) {
        std::cout << "Cleaning up GATT object..." << std::endl;
        //pGATTInterface->cleanup(); // This segfaults
        //pGATTInterface.reset();    // This also sefgaults!
        //...but without them I get a bus error???
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
    m_pGATTClientInterface.reset( m_pGATTInterface->client );
    m_pCommandAdapter = std::make_shared<FluorideBluetoothCommandGATTAdapter>( m_task, m_pBluetoothStack, m_pGATTServerInterface, m_pGATTClientInterface, m_btVendorInterface );
    p_gattthis = shared_from_this();
    m_pCommandAdapter->Initialize( cb );

    // NEW STUFF
    memset( s_client_uuid.uu, 0xDA, sizeof( s_client_uuid.uu ) );
    //m_pGATTClientInterface->register_client( &s_client_uuid );
    m_pGATTClientInterface->scan( true );
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





