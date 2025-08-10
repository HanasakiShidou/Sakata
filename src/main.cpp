// test_serial.cpp
#include "serial_port.hpp"
#include <iostream>

#include "protocol_interface.h"

using namespace std;

const char* TEST_DEVICE = "/dev/ttyACM0";  // 修改为实际设备路径
const char* TEST_STR = "Hello Serial!";
const int TEST_LEN = 12;

SerialPort serial;

int main() {
    uint32_t param = 1;
    CALL_1PARAM(DUMMY, param);
    
    if (!serial.open(TEST_DEVICE)) {
        cerr << "Failed to open serial port: " << strerror(errno) << endl;
        return 1;
    }

    //serial.set_blocking(false);
    serial.set_blocking(true);

    serial.close();
    return 0;
}