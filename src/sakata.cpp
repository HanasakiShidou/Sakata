#include <thread>
#include <chrono>
#include "sakata.hpp"

namespace Sakata
{

void FunctionManager::initialize() {
    // Note: Function implementation can be overridden by the parent.
    // REQUEST_NODENAME
    registerFunction({
        true,
        "REQUEST_NODENAME",
        ReservedFunctions::REQUEST_NODENAME,
        [parentNode = parentNode] (const RawData& input, RawData& output) -> bool {
            if (parentNode) {
                for (char ch: parentNode->nodeName) {
                    output.push_back(static_cast<uint8_t>(ch));
                }
            }
            return true;
        }
    });
    //REQUEST_FUNCTION_STATUS
    registerFunction({
        true,
        "REQUEST_FUNCTION_STATUS",
        ReservedFunctions::REQUEST_FUNCTION_STATUS,
        [&] (const RawData& input, RawData& output) -> bool {
            // input: funciton name -> char array
            std::string funcName(input.begin(), input.end());
            if (isFuncExist(funcName)) {
                auto funcId = getIndexByName(funcName);
                if (getFunc(funcId).valid) {
                    AppendVariableToRawData(FunctionStatus::STATUS_VALID, output);
                } else {
                    AppendVariableToRawData(FunctionStatus::STATUS_INVALID, output);
                }
            } else {
                AppendVariableToRawData(FunctionStatus::STATUS_NOT_EXIST, output);
            }
            return true;
        }
    });
    // TODO: REQUEST_AVAILBLE_FUNCTION
    // By any means the client should know what function they need to call.
}

FunctionID FunctionManager::getIndexByName(std::string name) {
    if (!isFuncExist(name)) {
        return ReservedFunctions::INVALID_FUNCTION;
    }
    return FunctionIndexingMap[name];
}

FunctionImplemention INVALID_FUNCTION_IMPLEMENTION;

FunctionImplemention& FunctionManager::getFunc(FunctionID funcId) {
    if (!isFuncExist(funcId)) {
        return INVALID_FUNCTION_IMPLEMENTION;
    }
    return FunctionMap[funcId];
}

FunctionImplemention& FunctionManager::getFunc(std::string name) {
    if (!isFuncExist(name)) {
        return INVALID_FUNCTION_IMPLEMENTION;
    }
    auto funcId = getIndexByName(name);
    return FunctionMap[funcId];
}

bool FunctionManager::registerFunction(FunctionImplemention info) {
    if (FunctionIndexingMap.count(info.functionName) > 0) {
        return false;
    }
    if (FunctionMap.count(info.index) > 0) {
        return false;
    }
    FunctionIndexingMap[info.functionName] = info.index;
    FunctionMap[info.index] = info;
    return true;
}

RequestSequenceNumber RemoteNode::getNewSeq() {
    return ++currentSq;
}

bool RemoteNode::processRequest() {
    bool ret = false;
    for (auto reqIt = outcomingCallRequest.begin(); reqIt != outcomingCallRequest.end(); reqIt++) {
        ret |= processOutcomingRequest(reqIt);
    }
    for (auto reqIt = incomingCallRequest.begin(); reqIt != incomingCallRequest.end(); reqIt++) {
        ret |= processIncomingRequest(reqIt);
    }
    return ret;
}

bool RemoteNode::processOutcomingRequest(std::vector<SakataRequest>::iterator reqIt) {
    auto& req = *reqIt;
    if (req.status != SakataRequest::Status::OUTCOMING_CREATED)
        return false;

    // Build the packet and send.
    Packet packet;
    packet.start = PACKET_START;
    packet.cmd = Commands::REQUEST;
    packet.sequence = 0;
    packet.dataLength = req.responseResult.size();
    packet.functionId = req.func.index;
    packet.requestSN = req.requestSN;
    packet.functionParameter = req.responseResult;
    packet.end = PACKET_END;

    auto ret = connection->Send(packet.serialize());
    if (!ret) {
        req.status = SakataRequest::Status::OUTCOMING_CALL_FAILED;
    } else {
        req.status = SakataRequest::Status::OUTCOMING_SENT;
    }
    return ret;
}

bool RemoteNode::handleResponsePacket(const Packet& packet) {
    if (packet.cmd != Commands::RESPONSE) {
        return false;
    }
    for (auto& request: outcomingCallRequest) {
        if (packet.requestSN == request.requestSN) {
            request.status = SakataRequest::Status::OUTCOMING_RESPONSED;
            request.responseResult = packet.functionResult;
            request.ready.release();
            return true;
        }
    }
    return false;
}

bool RemoteNode::processIncomingRequest(std::vector<SakataRequest>::iterator reqIt) {
    if (reqIt->status != SakataRequest::Status::INCOMING_CALL_FINISHED) {
        return false;
    }

    // Build response packet.
    Packet packet;
    packet.start = PACKET_START;
    packet.cmd = Commands::RESPONSE;
    packet.sequence = 0;
    packet.dataLength = reqIt->responseResult.size();
    packet.functionId = reqIt->func.index;
    packet.requestSN = reqIt->requestSN;
    packet.functionResult = reqIt->responseResult;
    packet.end = PACKET_END;

    auto ret = connection->Send(packet.serialize());
    if (!ret) {
        reqIt->status = SakataRequest::Status::INCOMING_RESPONSE_FAILED;
    } else {
        //reqIt->status = SakataRequest::Status::INCOMING_RESPONSED;
        reqIt->status = SakataRequest::Status::REQ_FINISHED;
    }
    return ret;
}

SakataCallResult RemoteNode::call(const FunctionInfo& funcInfo, const RawData parameter, bool synchronous = false) {
    // Build Request
    outcomingCallRequest.emplace_back(
        SakataRequest{
            SakataRequest::Status::OUTCOMING_CREATED,
            getNewSeq(),
            funcInfo,
            parameter,
            {},
            {},
            {},
        }
    );
    auto reqIt = outcomingCallRequest.end();

    if (synchronous) {
        // IF call synchronous, wait for CALL_TIME_OUT_TIME seconds.
        processOutcomingRequest(reqIt);
        int dealayedTime = 0;
        bool responed = false;
        while (dealayedTime < (CALL_TIME_OUT_TIME * 10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (reqIt->status == SakataRequest::Status::OUTCOMING_RESPONSED) {
                break;
            }
            dealayedTime++;
        }
    }
    return SakataCallResult(*reqIt);
}

void RemoteNode::onPacketIn(const RawData& rawdata) {
    auto packet = Packet::deSerialize(rawdata);
    if (packet.cmd == Commands::REQUEST) {
        if (parentNode) {
            parentNode->handleRequest(rawdata, *this);
        }
    } else if (packet.cmd == Commands::RESPONSE) {
        handleResponsePacket(packet);
    }
}

bool RemoteNode::releaseRequest() {
    bool ret = false;

    for (auto reqIt = outcomingCallRequest.begin(); reqIt != outcomingCallRequest.end(); reqIt++) {
        bool needToRelease = false;

        if (reqIt->status == SakataRequest::Status::OUTCOMING_CALL_FAILED) {
            needToRelease = true;
        }

        if (!reqIt->func.valid) {
            needToRelease = true;
        }

        if (reqIt->status == SakataRequest::Status::OUTCOMING_RESPONSED ||
            reqIt->status == SakataRequest::Status::REQ_FINISHED) {
            if (reqIt->done.acquire()) {
                needToRelease = true;
           }
        }

        if (needToRelease) {
            reqIt = outcomingCallRequest.erase(reqIt);
            ret |= true;
        }
    }

    for (auto reqIt = incomingCallRequest.begin(); reqIt != incomingCallRequest.end(); reqIt++) {
        bool needToRelease = false;

        switch (reqIt->status)
        {
            case SakataRequest::Status::INCOMING_RESPONSED:
            case SakataRequest::Status::INCOMING_CALL_FAILED:
            case SakataRequest::Status::INCOMING_RESPONSE_FAILED:
                needToRelease = true;
                break;

            case SakataRequest::Status::INCOMING_CREATED:
            case SakataRequest::Status::INCOMING_SENT:
            case SakataRequest::Status::INCOMING_RETRY:
            case SakataRequest::Status::INCOMING_CALL_FINISHED:
            default:
                //if (reqIt->done.acquire()) {
                //    needToRelease = true;
                //}
                needToRelease = true;
                break;
        }

        if (needToRelease) {
            reqIt = outcomingCallRequest.erase(reqIt);
            ret |= true;
        }
    }

    return ret;
}

RemoteNode::RemoteNode(std::shared_ptr<PointToPointConnection> connection_, SakataNode* parentNode_) :
    connection(connection_),
    parentNode(parentNode_),
    functions(this) {   
}

NodeID SakataNode::getIndexByName(std::string name) {
    return nodeIndexingMap.count(name) > 0 ? nodeIndexingMap[name] : -1;
}

RemoteNode* SakataNode::getNode(std::string name) {
    return nodeIndexingMap.count(name) <= 0 ? nullptr :
            (
                nodeMap.count(nodeIndexingMap.at(name)) <= 0 ? nullptr : &(nodeMap.at(nodeIndexingMap.at(name)))
            );
}

RemoteNode* SakataNode::getNode(NodeID nodeId) {
    return nodeMap.count(nodeId) > 0 ? &(nodeMap.at(nodeId)) : nullptr;
}

NodeID globalNodeId{0};

NodeID SakataNode::getNewNodeId() {
    globalNodeId++;
    return globalNodeId;
}

NodeID SakataNode::registerNode(std::shared_ptr<PointToPointConnection> connection_) {
    RemoteNode remoteNode(connection_, this);
    remoteNode.nodeId = getNewNodeId();

    // Note: current this is a uname node.
    //nodeIndexingMap[remoteNode.nodeName] = remoteNode.nodeId;
    nodeMap.emplace(remoteNode.nodeId, std::move(remoteNode));

    return remoteNode.nodeId;
}

bool SakataNode::handleRequest(const Packet& packet, RemoteNode& node) {
    if (packet.cmd == Commands::RESPONSE) {
        node.incomingCallRequest.emplace_back(
            SakataRequest{
                SakataRequest::Status::OUTCOMING_CREATED,
                packet.requestSN,
                functions.getFunc(packet.functionId),
                packet.functionResult,
                {},
                {},
                {},
            }
        );
        // TODO: Currently, there is no field indicating whether this call is synchronous or asynchronous.
        // TODO: A synchronous call need to be processed immediately.
        if (false) {
            auto it = node.incomingCallRequest.end();
            it--;
            node.processOutcomingRequest(it);
        }
    } else {
        return false;
    }
    // TODO: execption handling.
    return true;
}

bool SakataNode::handleCall(std::vector<SakataRequest>::iterator reqIt, RemoteNode& remoteNode) {
    if (reqIt->status != SakataRequest::Status::INCOMING_CREATED) {
        return false;
    }
    if (!functions.isFuncExist(reqIt->func.index) && functions.getFunc(reqIt->func.index).handler) {
        reqIt->status = SakataRequest::Status::INCOMING_CALL_FAILED;
        return false;
    } else {
        functions.getFunc(reqIt->func.index).handler(reqIt->requestParameter, reqIt->responseResult);
        reqIt->status = SakataRequest::Status::INCOMING_CALL_FINISHED;
        return true;
    }
}

bool SakataNode::processRemoteNodeRequests() {
    for (auto& [nodeId, remoteNode]: nodeMap) {
        remoteNode.work();
    }
}

}