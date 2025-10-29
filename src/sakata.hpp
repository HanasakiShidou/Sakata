#include <cstdint>
#include <cstddef>
#include <string>
#include <span>
#include <functional>

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
    PacketHeader* header;
    PacketBody body;
    bool valid{false};

    public:
    const PacketHeader& getHeader();
    const PacketBody& getBody();
    const bool isValid() {return valid;}

    bool buildFromRawData(const MemoryReference& data);
    Packet(const MemoryReference& data) {buildFromRawData(data);}
    Packet() = default;
};


class PointToPointConnection {
    public:
        inline const bool isActive() { return active; }
        inline void initailze(MemoryReferenceHandler handler) { if(handler) {on_send = handler; active = true;} }
        PointToPointConnection() = default;
        PointToPointConnection(MemoryReferenceHandler handler):on_send(handler), active(true) {}
    private:
        bool active{false};
        MemoryReferenceHandler on_send;
};


struct NodeInfo
{
    const std::string nodeName;
};


class SakataNode
{
    // Node information
    private:
    struct NodeInfo {
        const std::string nodeName;
    }nodeinfo;

    // Peer control and APIs.
    private:

    public:

    // Function management and APIs.
    private:

    public:

};


} // Namespace Sakata