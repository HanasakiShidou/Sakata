#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cassert>
#include "sakata.hpp"

#define TEST_Serialization

void checkPacketEqual(Sakata::Packet& base, Sakata::Packet& ref) {
    assert(base.isValid());
    assert(ref.isValid());
    assert(base.start              == ref.start            );
    assert(base.cmd                == ref.cmd              );
    assert(base.sequence           == ref.sequence         );
    assert(base.dataLength         == ref.dataLength       );
    assert(base.functionId         == ref.functionId       );
    assert(base.requestSN          == ref.requestSN        );
    assert(base.functionParameter  == ref.functionParameter);
    assert(base.functionResult     == ref.functionResult   );
}

int main() {

#ifdef TEST_Serialization

Sakata::Packet test_packet;

test_packet.start = Sakata::PACKET_START;
test_packet.valid = true;
test_packet.cmd   = Sakata::Commands::REQUEST;
test_packet.sequence = 10;
//test_packet.dataLength = 4 + 4 + 4;
test_packet.functionId = 11;
test_packet.requestSN = 12;
test_packet.functionParameter = {0x1, 0x11, 0x21, 0x31};
test_packet.end = Sakata::PACKET_END;

auto rawdata = test_packet.serialize();

auto rec_packet = Sakata::Packet::deSerialize(rawdata);

checkPacketEqual(test_packet, rec_packet);

#endif

    return 0;
}