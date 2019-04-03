#pragma once

#include <functional>
#include <memory>

#include <riviera_bt.hpp>


// Forward declaration for an internal-use structure
// Please pay this no mind!
class ConnectionData;


namespace RivieraGattClient {
    // Forward-declare connection class and give it a handy access alias
    class Connection;
    using ConnectionPtr = std::shared_ptr<Connection>;

    /** 
     * Type of function to call that handles info read from a characteristic
     * @param char* - buffer containing read data
     * @param size_t - length of data in buffer
     */  
    using ReadCallback = std::function<void(const char*, size_t)>; 
    
    /**
     * Type of function to call to determine if a delayed operation passed/failed
     * @param bool - true on success, false otherwise
     * @param std::string - reason for failure, if applicable and available
     */
    using StatusCallback = std::function<void(bool, std::string)>;
    
    /**
     * Connect to bluetooth peripheral
     * @param name: name of device to connect to. 
     * @param exact_match: If false, will connect to the first found device that starts with 'name'
     * @param timeout: timeout after this many seconds. Set negative for no timeout.
     * @return: Filled ConnectionPtr on success, nullptr on failure
     * TODO: needs arguments for what to connect to (presently hardcoded internally)
     */ 
    ConnectionPtr Connect(std::string name, bool exact_match=false, int timeout=-1);
    
    // Actual Connection class
    class Connection {
    public:
        //static ConnectionPtr Connect(std::string name, bool exact_match, int timeout);
        friend ConnectionPtr Connect(std::string name, bool exact_match, int timeout);
        
        // TODO: Do I need a destructor??
        // ~Connection();

        /**
         * Available or busy?
         * @return: true if available, false if busy
         */  
        bool Available();

        /**
         * Write to a characteristic of the connected device
         * @param uuid: 16-byte uuid of characteristic
         * @return: Non-zero on faulure, zero on success
         */
        int WriteCharacteristic(RivieraBT::UUID, std::string to_write);
        
        /**
         * Read from a characteristic of the connected device. Non-Blocking.
         * @param uuid: 16-byte uuid of characteristic
         * @param cb: callback function to handle data when it arrives. If null, no action is taken.
         * @return: zero on success, non-zero on failure
         */
        int ReadCharacteristic(RivieraBT::UUID uuid, ReadCallback cb=nullptr);

        /// block-wait version of the above
        std::string ReadCharacteristic(RivieraBT::UUID uuid);


        /**
         * Set max transmission size of connection. Size is a comprimise between both ends 
         * of a connection so check GetMaxTransmitSize before assuming!
         * @param xmit_size: new max transmit size
         * @param cb: callback function to alert that task is complete. If null, no action is taken.
         * @return: zero on success, non-zero on failure
         */
        int SetMaxTransmitSize(size_t xmit_size, StatusCallback cb=nullptr); 

        /**
         * Get max transmit size. Value is a comprimise between both ends of the connection
         * so it may be lower than the set value.
         * @return: Max transmit size of connection
         */ 
        size_t GetMaxTransmitSize();



        int GetConnectionID();


        std::string GetName();
        void PrintHandles();



    protected:
        //Connection(std::string name, int conn_id, bt_bdaddr_t* bda, std::atomic_bool& available_ref, 
        //    std::shared_ptr<btgatt_db_element_t>& uuid_db_ref, int& uuid_count_ref);    
        Connection(std::string name, int conn_id, bt_bdaddr_t* bda, ConnectionData* data);
        void fetch_services(void);
        void fill_handle_map(void);
        uint16_t find_characteristic_handle_from_uuid(RivieraBT::UUID uuid);

    private:
        std::string m_name;
        int m_conn_id;
        std::shared_ptr<bt_bdaddr_t> m_bda;
        std::shared_ptr<ConnectionData> m_data;
    };

}