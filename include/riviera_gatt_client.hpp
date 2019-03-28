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

    // Aliases for callback-types
    using ReadCallback = std::function<void(char*, size_t)>;
    //using ReadCallback = void(*)(void* buf, size_t len);
    
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
         * Read from a characteristic of the connected device. Blocking.
         * @param uuid: 16-byte uuid of characteristic
         * @return: string containing read data, empty on failure
         */
        std::string ReadCharacteristic(RivieraBT::UUID uuid);

        /**
         * Read from a characteristic of the connected device. Non-Blocking.
         * @param uuid: 16-byte uuid of characteristic
         * @param cb: callback function to handle data when it arrives
         * @return: zero on success, non-zero on failure
         */
        int ReadCharacteristic(RivieraBT::UUID uuid, ReadCallback cb);


        int GetConnectionID();


        std::string GetName();
        void PrintHandles();



    protected:
        //Connection(std::string name, int conn_id, bt_bdaddr_t* bda, std::atomic_bool& available_ref, 
        //    std::shared_ptr<btgatt_db_element_t>& uuid_db_ref, int& uuid_count_ref);    
        Connection(std::string name, int conn_id, bt_bdaddr_t* bda, ConnectionData* data);
        void fetch_services(void);
        void fill_handle_map(void);

    private:
        std::string m_name;
        int m_conn_id;
        std::shared_ptr<bt_bdaddr_t> m_bda;
        std::shared_ptr<ConnectionData> m_data;
    };

}