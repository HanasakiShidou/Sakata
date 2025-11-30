#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <cassert>
#include "sakata.hpp"

//#define TEST_Serialization
//#define TEST_PtP_Connection
#define TEST_Function_Call

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

#ifdef TEST_PtP_Connection

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

Sakata::RawData send_buffer;
Sakata::RawData receive_buffer;

//             -------->
// Local Point         |
//             <--------

Sakata::PointToPointConnection local_point;

local_point.onSend = [&](Sakata::RawData rawData) -> bool {
    send_buffer = rawData;
    return true;
};

local_point.onReceive = [&](Sakata::RawData rawData) -> bool {
    receive_buffer = rawData;
    return true;
};

local_point.Send(rawdata);
local_point.Receive(send_buffer);

auto rec_packet = Sakata::Packet::deSerialize(receive_buffer);

checkPacketEqual(test_packet, rec_packet);

#endif

#ifdef TEST_Function_Call

#endif

    Sakata::RawData serverBuffer;
    Sakata::RawData clientBuffer;

    Sakata::SakataNode server;
    Sakata::SakataNode client;

    server.nodeName = "SERVER";
    client.nodeName = "CLIENT";

    auto serverRemoteId = server.registerNode([&serverBuffer] (Sakata::RawData rawData) -> bool {
        serverBuffer = rawData;
        return true;
    });

    auto clientRemoteId = client.registerNode([&clientBuffer] (Sakata::RawData rawData) -> bool {
        clientBuffer = rawData;
        return true;
    });

    auto serverRemoteNodePtr = server.getNode(serverRemoteId);
    assert(serverRemoteNodePtr);

    auto FuncInfo = serverRemoteNodePtr->functions.getFuncInfo("REQUEST_NODENAME");

    Sakata::RawData response;
    Sakata::RequestSequenceNumber token;

    serverRemoteNodePtr->call(FuncInfo, {}, response, token);

    serverRemoteNodePtr->processRequest();

    client.onPacketIn(serverBuffer, clientRemoteId);
    client.SendResp();

    serverRemoteNodePtr->onPacketIn(clientBuffer);

    return 0;
}