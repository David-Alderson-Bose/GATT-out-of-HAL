

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

#include <memory> // in case I want to use shared_ptr
#include <iostream> // for debug print
#include <iomanip> // for extra debug print options




// ANdroind hal hardware structs
namespace { // anonymous namespace to prevent pollution
    
    bt_uuid_t s_client_uuid{};
    int s_client_if( -1 );
    bool client_registered = false;
    
    struct hw_device_t *pHWDevice;
    hw_module_t *pHwModule;
    bluetooth_device_t *pBTDevice;
    std::shared_ptr<bt_interface_t> pBluetoothStack;
    std::shared_ptr<btvendor_interface_t> btVendorInterface;


    void Shutdown() 
    {
        if (pBluetoothStack) {
            pBluetoothStack->disable();
            pBluetoothStack->cleanup();
        }
    }


    // Callback that triggers once client is registered
    void RegisterClientCallback(int status, int client_if, bt_uuid_t *app_uuid) {
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
        NULL, //RegisterClientCallback,
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
        NULL, //ConfigureMtuCallback, // configure_mtu_callback
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


    btgatt_callbacks_t sBTGATTCallbacks = {
        sizeof( sBTGATTCallbacks ),
        &sBTGATTClientCallbacks,
        NULL
    };
    
    /*
    static bt_callbacks_t sBluetoothCallbacks = {
        sizeof( sBluetoothCallbacks ),
        NULL, //AdapterStateChangeCb,
        NULL, //AdapterPropertiesCb,
        NULL, //RemoteDevicePropertiesCb,
        NULL, //DeviceFoundCb,
        NULL, //DiscoveryStateChangedCb,
        NULL, //PinRequestCb,
        NULL, //SspRequestCb,
        NULL, //BondStateChangedCb,
        NULL, //AclStateChangedCb,
        NULL, //CbThreadEvent,
        NULL, //DutModeRecvCb,
        NULL, //LeTestModeRecvCb,
        NULL, //EnergyInfoRecvCb,
        NULL, //HCIEventRecvCb,
    };
    */



static void AdapterStateChangeCb( bt_state_t state )
{
    std::cout << __func__ << ":" << __LINE__ << std::endl;
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


} // end anonymous namespace



    





extern   hw_module_t HAL_MODULE_INFO_SYM; // WWWWHYYYY ?????!
int BTSetup() 
{
    pHwModule = &HAL_MODULE_INFO_SYM;


    if( pHwModule->methods->open( pHwModule, BT_STACK_MODULE_ID, &pHWDevice ) )
    {
        std::cout << "BT Stack hw module retrieved." << std::endl;
    }

    pBTDevice = reinterpret_cast<bluetooth_device_t*>(pHWDevice);
    pBluetoothStack = std::make_shared<bt_interface_t>( *pBTDevice->get_bluetooth_interface() );

    if( !pBluetoothStack )
    {
        pBTDevice->common.close( ( hw_device_t * ) & pBTDevice->common );
        pBTDevice = NULL;
        Shutdown();
        std::cout << "IT BLEW UPPPP" << std::endl;
        return 1;
    }
    std::cout << "BT has been set UP." << std::endl;
    
    
    // Do I need to reimplement this? or can I leave it?
    // FluorideBluetoothDEVMAdapter::StackInitialize( m_pBluetoothStack );

    pBluetoothStack->init(&sBluetoothCallbacks);
    //pBluetoothStack->init(NULL);
    std::cout << "-1" << std::endl;
    pBluetoothStack->enable(true);
    std::cout << "0" << std::endl;
    
    //auto ptr = pBluetoothStack->get_profile_interface( BT_PROFILE_VENDOR_ID );
    //btVendorInterface.reset( ( btvendor_interface_t* )ptr );
    std::cout << "1" << std::endl;
    

    //m_logger.LogInfo( "Vendor interface: %p", ptr );
    //m_pDEVM_Adapter = std::make_shared<FluorideBluetoothDEVMAdapter>( m_pBluetoothStack, m_btVendorInterface, m_frontdoor, m_task );
    //m_ProfileAdapterList.push_back( m_pDEVM_Adapter );

    //m_pA2DP_Adapter = std::make_shared<FluorideBluetoothA2DPAdapter>( m_pBluetoothStack, m_pAudioServer );
    //m_ProfileAdapterList.push_back( m_pA2DP_Adapter );


    // THIS IS WHERE WE DIP INTO GATTLAND
    //m_pGATT_Adapter = std::make_shared<FluorideBluetoothGATTAdapter>( m_pBluetoothStack, m_btVendorInterface );
    
    
    pGATTInterface.reset( ( btgatt_interface_t* ) pBluetoothStack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    //pGATTInterface = std::make_shared<btgatt_interface_t*>(pBluetoothStack->get_profile_interface( BT_PROFILE_GATT_ID ) );
    std::cout << "1.5" << std::endl;
    if (!pGATTInterface) {
        std::cout << "GRABBIN THE GATT INTERFACE WENT SCREWBALLLZ" << std::endl;
        Shutdown();
        return 1;
    }
    pGATTInterface->init( &sBTGATTCallbacks );
    std::cout << "2" << std::endl;
    //pGATTServerInterface.reset( m_pGATTInterface->server );
    pGATTClientInterface.reset( pGATTInterface->client );
    std::cout << "3" << std::endl;
    
    // I dunno why uuid doesn't need to be filled out, but it works in CastleBluetooth...
    bt_status_t result = pGATTClientInterface->register_client( &s_client_uuid );
    
    std::cout << "registered client, waitin for acceptance" << std::endl;
    return 0;
}


int BTShutdown()
{
    if (!client_registered) {
        std::cout << "no client registered, nothing to shut down!" << std::endl;
        return 1;
    }
    
    pGATTClientInterface->unregister_client(s_client_if);
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





