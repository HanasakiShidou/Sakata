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
constexpr int CALL_TIME_OUT_TIME   = 5; // in second

enum class Commands : uint8_t {
    REQUEST,
    RESPONSE,
    RETRY,
    BUSY,
    FREE
};

// Basic data structs & types.

using FunctionID = int32_t;
using NodeID = int32_t;
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
    //private: -> TODO: for test
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
        bool Send(RawData rawData) { return onSend ? onSend(rawData) : false; }

        PointToPointConnection() = default; // TODO: -> for test
        PointToPointConnection(const PointToPointConnection&) = delete;
        PointToPointConnection& operator=(const PointToPointConnection&) = delete;
        PointToPointConnection(PointToPointConnection&&) = default;
        PointToPointConnection(RawDataHandler sendCallback, RawDataHandler receiveCallback) :
            onSend(sendCallback),
            onReceive(receiveCallback),
            active(sendCallback && receiveCallback) {}
    //private: -> TODO: for test
        bool active{false};
        RawDataHandler onSend;
        RawDataHandler onReceive;
};

struct BaseNode;

// Function management
// Using negative number as resrved ID.
enum ReservedFunctions : FunctionID {
    INVALID_FUNCTION          = -1,
    REQUEST_NODENAME          = -2,
    REQUEST_FUNCTION_STATUS   = -3,
    REQUEST_AVAILBLE_FUNCTION = -4,
};

enum FunctionStatus : int32_t {
    STATUS_INVALID = 0,
    STATUS_VALID,
    STATUS_NOT_EXIST
};

struct FunctionInfo {
    bool valid{false};
    std::string functionName;
    FunctionID index{-1};
};

struct FunctionImplemention : public FunctionInfo
{
    // Function implemention.
    std::function<bool(const RawData& input, RawData& output)> handler;
};

class FunctionManager 
{
    private:
    // For remote node, the handler is not implemented (as it should implemented in remote node).
    bool isRemote{false};
    std::unordered_map<std::string, FunctionID> FunctionIndexingMap;
    std::unordered_map<FunctionID, FunctionImplemention> FunctionMap;
    BaseNode* parentNode{nullptr};
    // Alloc resrved functions.
    inline void initialize();

    public:
    inline bool isFuncExist(std::string name) { 
        return FunctionIndexingMap.count(name) > 0 ? 
               FunctionMap.count(FunctionIndexingMap[name]) > 0:
               false;
    }
    inline bool isFuncExist(FunctionID funcId) { return FunctionMap.count(funcId) > 0; }
    FunctionID getIndexByName(std::string name);
    FunctionImplemention& getFunc(FunctionID funcId);
    FunctionImplemention& getFunc(std::string name);
    const FunctionInfo& getFuncInfo(FunctionID funcId) { return getFunc(funcId); }
    const FunctionInfo& getFuncInfo(std::string name) { return getFunc(name); }
    bool registerFunction(FunctionImplemention info);

    FunctionManager() = delete;
    FunctionManager(bool isRemote_) :isRemote(isRemote_) {}
    FunctionManager(bool isRemote_, BaseNode* parentNode_) :isRemote(isRemote_), parentNode(parentNode_) {}
};

struct SakataRequest {
    enum Status {
        // For outcoming.
        OUTCOMING_CREATED,
        OUTCOMING_SENT,
        OUTCOMING_RETRY,
        OUTCOMING_CALL_FAILED,
        OUTCOMING_RESPONSED,
        OUTCOMING_UNINIALIZED,
        // For incoming.
        INCOMING_CREATED,
        INCOMING_SENT,
        INCOMING_RETRY,
        INCOMING_CALL_FAILED,
        INCOMING_CALL_FINISHED,
        INCOMING_RESPONSED,
        INCOMING_UNINIALIZED,
        // For resource management.
        REQ_FINISHED,
    } status {OUTCOMING_UNINIALIZED};
    const RequestSequenceNumber requestSN{0};
    const FunctionInfo& func;
    RawData incomingBuffer{};
    RawData outcomingPayload{};
};

struct SakataRemoteRequest : public SakataRequest {
    NodeID remoteNodeId{0};
};

struct BaseNode
{
    NodeID nodeId{0};
    std::string nodeName;
};

class RemoteNode : public BaseNode
{
    private:
    PointToPointConnection connection;
    std::vector<SakataRequest> outcomingRequest;
    RequestSequenceNumber currentSequence{0};

    // true means a request is sent.
    bool processRequest(std::vector<SakataRequest>::iterator reqIt);

    // Function management and APIs.
    public:
    FunctionManager functions;

    // Assuming remote function calls are synchronous.
    bool call(const FunctionInfo& funcInfo, const RawData parameter, RawData& response, RequestSequenceNumber& token, bool synchronous = false);
    // For asynchronous call.
    bool getCallResponse(RequestSequenceNumber reqSN, RawData& response);

    // Get sequence.
    inline RequestSequenceNumber generateSequenceNumber() {
        currentSequence++;
        currentSequence = currentSequence < 0 ? 0 : currentSequence;
        return currentSequence;
    }

    public:
    bool handleResponsePacket(const Packet& packet);
    RemoteNode(PointToPointConnection&& connection_);
    RemoteNode(const RemoteNode&) = delete;
    RemoteNode& operator=(const RemoteNode&) = delete;
    RemoteNode(RemoteNode&&) = default;
    //RemoteNode() = delete;
    RemoteNode() = default;
};

class SakataNode : public BaseNode
{
    // Peer control and APIs.
    private:
    std::unordered_map<std::string, NodeID> nodeIndexingMap;
    std::unordered_map<NodeID, RemoteNode> nodeMap;
    NodeID currentRemoteNode{1};

    public:
    inline bool isNodeExist(std::string name) {
        return nodeIndexingMap.count(name) > 0 ?
               nodeMap.count(nodeIndexingMap[name]) > 0:
               false;
    }
    inline bool isNodeExist(NodeID nodeId) { return nodeMap.count(nodeId) > 0; }
    NodeID getIndexByName(std::string name);
    RemoteNode* getNode(std::string name);
    RemoteNode* getNode(NodeID nodeId);
    bool registerNode(PointToPointConnection&& connection);

    // Incoming reqest to this node.
    private:
    std::vector<SakataRemoteRequest> incomingRequest;

    // In/Out conn
    public:
    void onPacketIn(const RawData& rawdata, NodeID nodeid);

    // Function management and APIs.
    private:
    FunctionManager functions;
    bool handleCall(std::vector<SakataRemoteRequest>::iterator reqIt);
    bool SendResp(std::vector<SakataRemoteRequest>::iterator reqIt);

    public:
    bool registerFunction(FunctionImplemention info) { return functions.registerFunction(info); }

};


} // Namespace Sakata