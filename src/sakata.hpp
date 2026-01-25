#include "packet.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <span>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Sakata
{

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
    // We can get this info from parent node.
    //bool isRemote{false};
    std::unordered_map<std::string, FunctionID> FunctionIndexingMap;
    std::unordered_map<FunctionID, FunctionImplemention> FunctionMap;
    BaseNode* parentNode{nullptr};
    // Alloc resrved functions.
    void initialize();

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

    FunctionManager() = default;
    FunctionManager(BaseNode* parentNode_) : parentNode(parentNode_) {
        initialize();
    }
};

struct Signal {
    private:
        std::atomic<bool> signal_{false};
    public:
        void inline release() {
            signal_.store(true, std::memory_order_release);
        }
        bool inline acquire() {
            return signal_.load(std::memory_order_acquire);
        }
    };

struct SakataRequest {
    enum class Status {
        // For outcoming.
        OUTCOMING_CREATED,
        OUTCOMING_SENT,
        OUTCOMING_RETRY,
        OUTCOMING_CALL_FAILED,
        OUTCOMING_RESPONSED,
        //OUTCOMING_UNINIALIZED,
        // For incoming.
        INCOMING_CREATED,
        INCOMING_SENT,
        INCOMING_RETRY,
        INCOMING_CALL_FAILED,
        INCOMING_CALL_FINISHED,
        INCOMING_RESPONSE_FAILED,
        INCOMING_RESPONSED,
        //INCOMING_UNINIALIZED,
        // For resource management.
        REQ_UNINIALIZED,
        REQ_FINISHED,
    } status {Status::REQ_UNINIALIZED};
    const RequestSequenceNumber requestSN{0};
    const FunctionInfo& func;
    // Function call parameter buffer
    RawData requestParameter{};
    // Responsed function call result.
    RawData responseResult{};
    
    // To info the consumer is able to read the data
    Signal ready;
    // To info the porducer is able to release this request
    Signal done;
};

class SakataCallResult {
    SakataRequest& request;
    public:
    SakataCallResult(SakataRequest& request_) :request(request_) {}
    inline bool isDone() {
        return request.ready.acquire() == true;
    }
    // template
    const RawData& getRawData() { return request.responseResult; }
    // When result read is done, the node can release the resource.
    ~SakataCallResult() {
        request.status = SakataRequest::Status::REQ_FINISHED;
        request.done.release();
    }
};

struct BaseNode
{
    NodeID nodeId{0};
    std::string nodeName{"DEFAULT"};
};

class SakataNode;

// Remote endpoints
class RemoteNode : public BaseNode
{
    private:
    std::shared_ptr<PointToPointConnection> connection{nullptr};
    std::vector<SakataRequest> outcomingCallRequest;
    std::vector<SakataRequest> incomingCallRequest;
    SakataNode* parentNode;
    RequestSequenceNumber currentSq{0};

    // true means a request is sent.
    bool processIncomingRequest(std::vector<SakataRequest>::iterator reqIt);
    bool processOutcomingRequest(std::vector<SakataRequest>::iterator reqIt);
    bool releaseRequest();
    // true means a request is sent.
    bool processRequest();
    bool handleResponsePacket(const Packet& packet);

    public:
    // connection call back.
    void onPacketIn(const RawData& rawdata);

    // Function management and APIs.
    public:
    FunctionManager functions;

    // Assuming remote function calls are synchronous.
    SakataCallResult call(const FunctionInfo& funcInfo, const RawData parameter, bool synchronous = false);
    // For asynchronous call.
    //bool getCallResponse(RequestSequenceNumber reqSN, RawData& response);

    private:
    // Get sequence.
    RequestSequenceNumber getNewSeq();

    public:
    inline void work() { processRequest(); }

    RemoteNode(std::shared_ptr<PointToPointConnection> connection_, SakataNode* parentNode_);
    RemoteNode(const RemoteNode&) = delete;
    RemoteNode& operator=(const RemoteNode&) = delete;
    RemoteNode(RemoteNode&&) = default;
    //RemoteNode() = delete;
    RemoteNode() = default;
    friend class SakataNode;
};

// Local node
// A remote node should be strictly bound to a peer-to-peer link. 
// Managing peer-to-peer link is the user's responsibility.
class SakataNode : public BaseNode
{
    // Peer control and APIs.
    private:
    std::unordered_map<std::string, NodeID> nodeIndexingMap;
    std::unordered_map<NodeID, RemoteNode> nodeMap;
    NodeID getNewNodeId();

    // Node management
    public:
    inline bool isNodeExist(std::string name) {
        return nodeIndexingMap.count(name) > 0 ?
               nodeMap.count(nodeIndexingMap[name]) > 0:
               false;
    }
    inline bool isNodeExist(NodeID nodeId) { return nodeId == 0 ? true : nodeMap.count(nodeId) > 0; }
    NodeID getIndexByName(std::string name);
    RemoteNode* getNode(std::string name);
    RemoteNode* getNode(NodeID nodeId);
    NodeID registerNode(std::shared_ptr<PointToPointConnection> connection_);

    // Function management and APIs.
    private:
    FunctionManager functions;
    bool handleCall(std::vector<SakataRequest>::iterator reqIt, RemoteNode& remoteNode);

    public:
    bool registerFunction(FunctionImplemention info) { return functions.registerFunction(info); }

    bool processRemoteNodeRequests();
    public:
    // Thread entrypoint.
    inline void work() { processRemoteNodeRequests(); }

    // Remote node interface
    public:
    bool handleRequest(const Packet& packet, RemoteNode& node);

    SakataNode() : functions(this) {};
};

// Simple node

} // Namespace Sakata