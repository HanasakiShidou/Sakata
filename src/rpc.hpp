#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include <span>
#include <array>

using Handler =  std::function<void(const std::span<uint8_t>, const uint16_t)>;
using RPCHandler =  std::function<void(const std::span<uint8_t>)>;

constexpr int32_t DYNSIZE = -1;

struct RPCHandlerInfo {
public:
    int32_t paramSize{0};
    RPCHandler handler{nullptr};
};

class RPC {
private:
    Handler handler{nullptr};
    using CmdType = uint8_t;
    std::array<size_t,  256 * sizeof(CmdType)> funcInfo{0};
    bool privateCmd(CmdType cmd) { return cmd & 0x80 != 0; }

public:
    // API
    void incomingCall(const std::span<uint8_t> data, const uint16_t packet_id) {

    }

    void setOutingCall(Handler outcomingCall) { handler = outcomingCall; }
    
};