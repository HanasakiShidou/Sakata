// test_serial.cpp
#include "serial_port.hpp"
#include <iostream>
#include <iomanip>
#include <vector>

#include "protocol_interface.h"

using namespace std;

void FAKE_RX_DATA(const void* data, uint32_t len) {
    std::cout << "Debug: On fake rx data, recev " << len << " bytes:";
    std::cout << std::hex << std::setw(2) << std::endl;
    for (auto ptr = 0; ptr < len; ptr++) {
        std::cout << std::setw(2) << std::setfill('0') <<  (int)(reinterpret_cast<const uint8_t*>(data))[ptr] << ' ';
        if ((ptr + 1) % 4 == 0) 
            std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
}

//void protocol_send_request(const uint8_t* data, uint32_t len) {
//    protocol_on_rx_data(data, len);
//}

uint64_t DUMMY_PROC(uint64_t val) {
    std::cout << "DUMMY_PROC: called with param: " 
        << std::hex << val << std::dec << std::endl;
    return val;
}

//DEFINE_PROCESS_1PARAM(DUMMY_PROC, DUMMY, uint64_t)

int main() {
    //protocol_init(FAKE_RX_DATA);
    //REGISTER_PROCESS_1PARAM(DUMMY_PROC, DUMMY);

    uint64_t param = 1;
    //CALL_1PARAM(DUMMY, param);
 
    return 0;
}