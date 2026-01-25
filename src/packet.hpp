#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <functional>

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
using RawDataView = std::span<const uint8_t>;


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

template<typename T>
void AppendVariableToRawData(T var, RawData& rawData) {
    std::array<uint8_t, sizeof(T)> byteArray;
    std::memcpy(byteArray.data(), &var, sizeof(byteArray));
    rawData.insert(rawData.end(), byteArray.begin(), byteArray.end());
}

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

} // Namespace Sakata