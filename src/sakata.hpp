#include <cstdint>
#include <cstddef>
#include <string>
#include <span>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Sakata
{

// Constant values.
    
constexpr uint8_t PACKET_START     = 0xAA;
constexpr uint8_t PACKET_END       = 0x55;
constexpr size_t  MAX_PAYLOAD      = 256;
constexpr size_t  MAX_BUFFERSIZE   = 1024;

enum class Commands : uint8_t {
    REQUEST,
    RESPONSE,
    RETRY,
    BUSY,
    FREE
};

//  Negative value are reserved.
enum ReservedFunctionID : int32_t {
    REQUEST_NODENAME = -1,
};

// Basic data structs & types.

using FunctionID = int32_t;
using RequestSequenceNumber = int32_t;
using RawData = std::vector<uint8_t>;
using RawDataHandler = std::function<bool(RawData)>;

/*
* PACKET STRUCTURE
* [] -> Necessary fields.
* <> -> Optional fields.
*
* Current packet structure:
* [START 1byte] [COMMANDS 1btye] [SEQUENCE 2byte] [LENGTH 2byte]
* <FUNCTION_ID 4byte>
* <REQUEST_SEQ_NUM 4byte>
* <FUNCTION_RESULT dynamic length>
* <FUNCTION_PARAMETER dynamic length>
* [END 1byte]
*/

struct PacketHeader {
    uint8_t    start;
    Commands   cmd;
    int16_t    sequence;
    int16_t    dataLength;
};

// The length of data members is dynamic and needs to be manually initialized before use
struct PacketBody
{
    // Optional fields
    // Function and parameters
    FunctionID functionId{-1};
    RequestSequenceNumber requestSN{0};
    RawData functionParameter;
    RawData functionResult;
    
    // XOR for whole packet, reserved now.
    //uint8_t checksum{0};   
    uint8_t end;
};


class Packet : public PacketHeader, public PacketBody
{
    //private:
    public:
    bool valid{false};

    public:
    const PacketHeader& getHeader() { return *this; }
    const PacketBody& getBody() { return *this; }
    const bool isValid() { return valid; }

    static Packet deSerialize(const RawData& data);
    const RawData serialize();
    Packet(const RawData& data) { deSerialize(data); }
    Packet() = default;
};

// Raw data transfer layer.
class PointToPointConnection {
    public:
        inline const bool isActive() { return active; }
        inline void initialize(RawDataHandler sendCallback, RawDataHandler receiveCallback) {
            if(sendCallback && receiveCallback) { onSend = sendCallback, onReceive = receiveCallback; active = true; } 
        }

        inline RawDataHandler getSendCallback() {
            return onSend ? onSend : nullptr;
        }

        inline RawDataHandler getReceiveCallback() {
            return onReceive ? onReceive : nullptr;
        }

        bool Receive(RawData rawData) { return onReceive ? onReceive(rawData) : false; }

        // PointToPointConnection() = default;
        PointToPointConnection(const PointToPointConnection&) = delete;
        PointToPointConnection& operator=(const PointToPointConnection&) = delete;
        PointToPointConnection(PointToPointConnection&&) = default;
        PointToPointConnection(RawDataHandler sendCallback, RawDataHandler receiveCallback) :
            onSend(sendCallback),
            onReceive(receiveCallback),
            active(sendCallback && receiveCallback) {}
    private:
        bool active{false};
        RawDataHandler onSend;
        RawDataHandler onReceive;
};


struct NodeInfo
{
    const std::string nodeName;
};

class RemoteNode : public NodeInfo
{
    private:
    PointToPointConnection connection;

    public:
    RemoteNode(PointToPointConnection&& connection_) : connection(std::move(connection_)){}
    RemoteNode(const RemoteNode&) = delete;
    RemoteNode& operator=(const RemoteNode&) = delete;
    RemoteNode(RemoteNode&&) = default;
    RemoteNode() = delete;
};

//RemoteNode initializeNode(PointToPointConnection connection) {
//
//}

class SakataNode : public NodeInfo
{
    // Peer control and APIs.
    private:
    std::unordered_map<std::string, PointToPointConnection> nodeMap;

    public:
    bool registerNodeMap(RawDataHandler handler);

    // Function management and APIs.
    private:

    public:

};


} // Namespace Sakata