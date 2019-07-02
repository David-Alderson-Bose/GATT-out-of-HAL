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

        /**
         * TODO
         */ 
        int GetConnectionID();

        /**
         * Set maximum transmission unit (MTU)
         * @param new_mtu: New MTU to set on the connection
         * @return: zero on success, non-zero on failure 
         */
        int SetMTU(unsigned int new_mtu); 

        /**
         * Get maximum transmission unit (MTU)
         * @return: MTU for the connection
         */
        unsigned int GetMTU();

        /**
         * TODO
         */ 
        std::string GetName();
        
        /**
         * TODO
         */ 
        void PrintHandles();

    protected:
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