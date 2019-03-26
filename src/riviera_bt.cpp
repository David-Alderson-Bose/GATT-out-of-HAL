
#include <memory> // in case I want to use shared_ptr
#include <iostream> // for debug print
#include <iomanip> // for extra debug print options
#include <atomic>
#include <sstream>
#include <map>
#include <algorithm> // for equal
#include <iterator>


#include <riviera_bt.hpp>


std::string RivieraBT::StringifyUUID(UUID uuid) {
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


std::string RivieraBT::StringifyUUID(bt_uuid_t* uuid) {
    if (!uuid) {
        std::cerr << __func__ << ": Empty bt_uuid_t" << std::endl;
        return std::string("");
    }
    return RivieraBT::StringifyUUID((uuid->uu));
}


int RivieraBT::Setup() 
{
    if (RivieraBT::isSetup()) {
        return 0; // Nothing to do!
    }
    // TODO: Actual work
    return 0;
}


bool RivieraBT::isSetup()
{
    // TODO: Actual work
    return 0;
}


RivieraBT::GattPtr RivieraBT::GetGatt()
{
    // TODO: Actual work
    return nullptr;
}