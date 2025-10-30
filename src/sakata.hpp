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
using MemoryReference = std::span<uint8_t>;
using MemoryReferenceHandler = std::function<bool(MemoryReference)>;

#pragma pack(push, 1)
struct PacketHeader {
    uint8_t    start;
    Commands   cmd;
    int16_t    frameId;
    int16_t    dataLength;
};
#pragma pack(pop)


// The length of data members is dynamic and needs to be manually initialized before use
struct PacketBody
{
    // Function and parameters
    FunctionID functionId{-1};
    MemoryReference functionParameter;
    
    uint8_t end;
    // XOR for whole packet, reserved now.
    //uint8_t checksum{0};   
};


class Packet
{
    private:
    PacketHeader header;
    PacketBody body;
    bool valid{false};
    // Nullptr if this packet does not have the ownership of the raw data.
    std::unique_ptr<uint8_t> rawData{nullptr};

    public:
    const PacketHeader& getHeader() { return header; }
    const PacketBody& getBody() { return body; }
    const bool isValid() { return valid; }

    static Packet deSerialization(const MemoryReference& data);
    const MemoryReference Serialization();
    Packet(const MemoryReference& data) { deSerialization(data); }
    Packet() = default;
};

// Raw data transfer layer.
class PointToPointConnection {
    public:
        inline const bool isActive() { return active; }
        inline void initialize(MemoryReferenceHandler sendCallback, MemoryReferenceHandler receiveCallback) {
            if(sendCallback && receiveCallback) { onSend = sendCallback, onReceive = receiveCallback; active = true; } 
        }

        inline MemoryReferenceHandler getSendCallback() {
            return onSend ? onSend : nullptr;
        }

        inline MemoryReferenceHandler getReceiveCallback() {
            return onReceive ? onReceive : nullptr;
        }

        bool Receive(MemoryReference rawData) { return onReceive ? onReceive(rawData) : false; }

        // PointToPointConnection() = default;
        PointToPointConnection(const PointToPointConnection&) = delete;
        PointToPointConnection& operator=(const PointToPointConnection&) = delete;
        PointToPointConnection(PointToPointConnection&&) = default;
        PointToPointConnection(MemoryReferenceHandler sendCallback, MemoryReferenceHandler receiveCallback) :
            onSend(sendCallback),
            onReceive(receiveCallback),
            active(sendCallback && receiveCallback) {}
    private:
        bool active{false};
        MemoryReferenceHandler onSend;
        MemoryReferenceHandler onReceive;
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

class SakataNode : public NodeInfo
{
    // Peer control and APIs.
    private:
    std::unordered_map<std::string, PointToPointConnection> nodeMap;

    public:
    bool registerNodeMap(MemoryReferenceHandler handler);

    // Function management and APIs.
    private:

    public:

};


} // Namespace Sakata