#include <cstring>
#include <thread>
#include <chrono>
#include "sakata.hpp"

namespace Sakata
{

Packet Packet::deSerialize(const RawData& rawData) {
    Packet packet;

    // Find start & end flag.
    auto startIt = std::find(rawData.begin(), rawData.end(), Sakata::PACKET_START);
    auto endIt = std::find(rawData.rbegin(), rawData.rend(), Sakata::PACKET_END).base();
    
    if (startIt == rawData.end() || endIt == rawData.begin()) {
        return packet;
    }
    
    std::span rawPacket(startIt, endIt);

    if (rawPacket.back() != Sakata::PACKET_END) {
        return packet;
    }

    // Minimum size for a packet.
    constexpr size_t MINIMUM_DATA_SIZE = sizeof(PACKET_START) +
                                         sizeof(PacketHeader::cmd) +
                                         sizeof(PacketHeader::sequence) +
                                         sizeof(PacketHeader::dataLength) +
                                         sizeof(PACKET_END);;
    if (rawPacket.size() < MINIMUM_DATA_SIZE) {
        return packet;
    }

    // Check length for optional fields.
    size_t dataOffset = 0;
    
    packet.start = rawPacket[dataOffset];
    dataOffset += sizeof(PacketHeader::start);
    
    packet.cmd = static_cast<Sakata::Commands>(rawPacket[dataOffset]);
    dataOffset += sizeof(PacketHeader::cmd);

    // Using little endian here.
    std::memcpy(&packet.sequence, &(rawPacket[dataOffset]), sizeof(PacketHeader::sequence));
    dataOffset += sizeof(PacketHeader::sequence);

    std::memcpy(&packet.dataLength, &(rawPacket[dataOffset]), sizeof(PacketHeader::dataLength));
    dataOffset += sizeof(PacketHeader::dataLength);

    // Note: All data in PacketBody is not verified, we only copy data here.
    // Verify packet length.
    if ((packet.dataLength + MINIMUM_DATA_SIZE) != rawPacket.size()) {
        return packet;
    }

    // Check pass, start copy optional fields (based on packet header).
    packet.valid = true;
    switch (packet.cmd)
    {
    case Commands::REQUEST:
    {
        std::memcpy(&packet.functionId, &(rawPacket[dataOffset]), sizeof(PacketBody::functionId));
        dataOffset += sizeof(PacketBody::functionId);

        std::memcpy(&packet.requestSN, &(rawPacket[dataOffset]), sizeof(PacketBody::requestSN));
        dataOffset += sizeof(PacketBody::requestSN);
        
        // Copy data if the dataLength indicates we have a functionParameter.
        constexpr size_t REQUEST_STATIC_PART_LENGTH = sizeof(PacketBody::functionId) + sizeof(PacketBody::requestSN);
        if (packet.dataLength > REQUEST_STATIC_PART_LENGTH) {
            packet.functionParameter = RawData(rawPacket.begin() + dataOffset,
                                                    rawPacket.begin() + dataOffset +(packet.dataLength - REQUEST_STATIC_PART_LENGTH));
        }
        break;
    }

    case Commands::RESPONSE:
    {
        std::memcpy(&packet.functionId, &(rawPacket[dataOffset]), sizeof(PacketBody::functionId));
        dataOffset += sizeof(PacketBody::functionId);

        std::memcpy(&packet.requestSN, &(rawPacket[dataOffset]), sizeof(PacketBody::requestSN));
        dataOffset += sizeof(PacketBody::requestSN);

        // Copy data if the dataLength indicates we have a functionResult.
        constexpr size_t REQUEST_STATIC_PART_LENGTH = sizeof(PacketBody::functionId) + sizeof(PacketBody::requestSN);
        if (packet.dataLength > REQUEST_STATIC_PART_LENGTH) {
            packet.functionResult = RawData(rawPacket.begin() + dataOffset,
                                                 rawPacket.begin() + dataOffset +(packet.dataLength - REQUEST_STATIC_PART_LENGTH));
        }
        break;
    }

    default:
    break;
    }
            
    // TODO: Check XOR checksum.
    //uint8_t checksum = 0;
    //for (size_t i = 1; i < frame.size() - 2; ++i) { // 排除起始和校验、结束
    //    checksum ^= frame[i];
    //}
    //if (checksum != frame[frame.size() - 2]) {
    //    return false;
    //}
    return packet;
}

template<typename T>
void AppendVariableToRawData(T var, RawData& rawData) {
    std::array<uint8_t, sizeof(T)> byteArray;
    std::memcpy(byteArray.data(), &var, sizeof(byteArray));
    rawData.insert(rawData.end(), byteArray.begin(), byteArray.end());
}

const RawData Packet::serialize() {
    RawData rawData;

    // Serialize packet header.
    rawData.push_back(Sakata::PACKET_START);
    rawData.push_back(static_cast<uint8_t>(cmd));
    AppendVariableToRawData(sequence, rawData);

    // Data length is calculated here.
    RawData optionalFields;
    switch (cmd)
    {
    case Commands::REQUEST:
    {
        AppendVariableToRawData(functionId, optionalFields);
        AppendVariableToRawData(requestSN, optionalFields);
        // Copy data if we have a functionParameter.
        if (functionParameter.size() > 0) {
            optionalFields.insert(optionalFields.end(), functionParameter.begin(), functionParameter.end());
        }
        break;
    }

    case Commands::RESPONSE:
    {
        AppendVariableToRawData(functionId, optionalFields);
        AppendVariableToRawData(requestSN, optionalFields);
        // Copy data if we have a functionParameter.
        if (functionResult.size() > 0) {
            optionalFields.insert(optionalFields.end(), functionResult.begin(), functionResult.end());
        }
        break;
    }

    default:
    break;
    }

    // dataLength is 16bit int.
    dataLength = static_cast<typeof(dataLength)>(optionalFields.size());
    AppendVariableToRawData(dataLength, rawData);

    rawData.insert(rawData.end(), optionalFields.begin(), optionalFields.end());

    rawData.push_back(Sakata::PACKET_END);

    return rawData;
}

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

bool RemoteNode::processRequest(std::vector<SakataRequest>::iterator reqIt) {
    auto& req = *reqIt;
    if (req.status != SakataRequest::OUTCOMING_CREATED)
        return;

    // Build the packet and send.
    Packet packet;
    packet.start = PACKET_START;
    packet.cmd = Commands::REQUEST;
    packet.sequence = 0;
    packet.dataLength = req.outcomingPayload.size();
    packet.functionId = req.func.index;
    packet.requestSN = req.requestSN;
    packet.functionParameter = req.outcomingPayload;
    packet.end = PACKET_END;

    auto ret = connection.Send(packet.serialize());
    if (!ret) {
        req.status = SakataRequest::OUTCOMING_CALL_FAILED;
    } else {
        req.status = SakataRequest::OUTCOMING_SENT;
    }
    return ret;
}

bool RemoteNode::handleResponsePacket(const Packet& packet) {
    if (packet.cmd != Commands::RESPONSE) {
        return false;
    }
    for (auto& request: outcomingRequest) {
        if (packet.requestSN == request.requestSN) {
            request.status = SakataRequest::OUTCOMING_RESPONSED;
            request.incomingBuffer = packet.functionResult;
        }
    }
}

bool RemoteNode::getCallResponse(RequestSequenceNumber reqSN, RawData& response) {
    for (auto& request: outcomingRequest) {
        if (reqSN == request.requestSN) {
            if (request.status == SakataRequest::OUTCOMING_RESPONSED) {
                response = request.incomingBuffer;
                request.status = SakataRequest::REQ_FINISHED;
                return false;
            } else {
                return false;
            }
        }
    }
    return false;
}

bool RemoteNode::call(const FunctionInfo& funcInfo, const RawData parameter, RawData& response, RequestSequenceNumber& token, bool synchronous = false) {
    // Ignore invalid call
    if (!funcInfo.valid) {
        return false;
    }
    
    // Build Request
    auto reqIt = outcomingRequest.insert(
        outcomingRequest.end(),
        SakataRequest({
            .status = SakataRequest::OUTCOMING_CREATED,
            .requestSN = generateSequenceNumber(),
            .func = funcInfo,
            .outcomingPayload = parameter,
        })
    );

    if (synchronous) {
        // IF call synchronous, wait for CALL_TIME_OUT_TIME seconds.
        processRequest(reqIt);
        int dealayedTime = 0;
        bool responed = false;
        while (dealayedTime < (CALL_TIME_OUT_TIME * 10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (responed = getCallResponse(reqIt->requestSN, response)) {
                reqIt->status = SakataRequest::REQ_FINISHED;
                break;
            }
            dealayedTime++;
        }
        return responed;
    } else {
        token = reqIt->requestSN;
    }
    return true;
}

RemoteNode::RemoteNode(PointToPointConnection&& connection_) :
connection(std::move(connection_)), functions(true, this) {
    
}

NodeID SakataNode::getIndexByName(std::string name) {
    return nodeIndexingMap.count(name) > 0 ? nodeIndexingMap[name] : -1;
}

RemoteNode* SakataNode::getNode(std::string name) {
    return nodeIndexingMap.count(name) <= 0 ? nullptr :
            (
                nodeMap.count(nodeIndexingMap[name]) <= 0 ? nullptr : &(nodeMap[nodeIndexingMap[name]])
            );
}

RemoteNode* SakataNode::getNode(NodeID nodeId) {
    return nodeMap.count(nodeId) > 0 ? &(nodeMap[nodeId]) : nullptr;
}

bool SakataNode::registerNode(PointToPointConnection&& connection) {
    RemoteNode remoteNode(std::move(connection));
    RawData response;
    RequestSequenceNumber token{0};
    bool succeeded = remoteNode.call(
        remoteNode.functions.getFuncInfo("REQUEST_NODENAME"),
        {},
        response,
        token,
        true
    );
    if (!succeeded) {
        return false;
    }

    std::string remoteNodeName(response.begin(), response.end());
    remoteNode.nodeName = remoteNodeName;
    remoteNode.nodeId = currentRemoteNode++;

    nodeIndexingMap[remoteNodeName] = remoteNode.nodeId;
    nodeMap.emplace(remoteNode.nodeId, std::move(remoteNode));

    return true;
}

void SakataNode::onPacketIn(const RawData& rawdata, NodeID nodeId) {
    if (!isNodeExist(nodeId))
        return;

    auto packet = Packet::deSerialize(rawdata);
    if (packet.cmd == Commands::REQUEST) {
        //handleCall(packet, nodeId);
        SakataRemoteRequest remoteRequest({
            .status = SakataRemoteRequest::INCOMING_CREATED,
            .requestSN = packet.requestSN,
            .func = functions.getFunc(packet.functionId),
            .incomingBuffer = packet.functionResult,
        });
        remoteRequest.remoteNodeId = nodeId;
        auto it = incomingRequest.insert(incomingRequest.end(), std::move(remoteRequest));
        handleCall(it);
    } else if (packet.cmd == Commands::RESPONSE) {
        nodeMap.at(nodeId).handleResponsePacket(packet);
    }

    // TODO: execption handling.
}


bool SakataNode::handleCall(std::vector<SakataRemoteRequest>::iterator reqIt) {
    if (reqIt->status != SakataRemoteRequest::INCOMING_CREATED) {
        return false;
    }
    if (!functions.isFuncExist(reqIt->func.index) && functions.getFunc(reqIt->func.index).handler) {
        reqIt->status = SakataRemoteRequest::INCOMING_CALL_FAILED;
        return false;
    } else {
        auto& functionImplemention = functions.getFunc(reqIt->func.index);
        functionImplemention.handler(reqIt->incomingBuffer, reqIt->outcomingPayload);
        reqIt->status = SakataRemoteRequest::INCOMING_CALL_FINISHED;
        return true;
    }
}

bool SakataNode::handleCall(std::vector<SakataRemoteRequest>::iterator reqIt) {
    // TODO:
    return true;
}

}