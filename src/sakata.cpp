#include <cstring>
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

}