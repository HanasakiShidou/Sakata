#include "protocol_interface.h"
#include "protocol_engine.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化协议栈
void protocol_init(void (*callback)(const void*, uint32_t)) {
    // 服务端处理回调示例
    Protocol::ProtocolEngine::instance().set_server_handler(
        [](const std::vector<uint8_t>& payload, uint16_t frame_id) -> std::vector<uint8_t> {
            return payload;
        }
    );

    Protocol::ProtocolEngine::instance().set_send_callback([callback=callback](const std::vector<uint8_t>& bytes, uint16_t len) { if (callback) callback(bytes.data(), bytes.size());});
}


// 接收数据处理
void protocol_on_rx_data(const uint8_t* data, uint32_t len) {
    Protocol::ProtocolEngine::instance().process_rx_data(data, len);
}


// 发送请求（客户端）
void protocol_send_request(const uint8_t* data, uint32_t len) {
    std::vector<uint8_t> payload(data, data + len);
    auto frame = Protocol::ProtocolEngine::buildFrame(Protocol::CmdType::REQUEST, Protocol::ProtocolEngine::instance().current_frame_id, payload);
    //Protocol::ProtocolEngine::instance().send_raw_frame(frame);
    protocol_on_rx_data(frame.data(), frame.size());
}


// 发送响应（服务端）
void protocol_send_response(const uint8_t* data, size_t len) {
    std::vector<uint8_t> payload(data, data + len);
    auto frame = Protocol::ProtocolEngine::buildFrame(Protocol::CmdType::RESPONSE, Protocol::ProtocolEngine::instance().current_frame_id, payload);
    Protocol::ProtocolEngine::instance().send_raw_frame(frame);
}

#ifdef __cplusplus
}
#endif

std::array<uint32_t(*)(const uint8_t* const, int32_t), ProcessList::ALL_PROECESS> ProcessMap{nullptr};

void register_function(enum ProcessList process, uint32_t (*handler)(const uint8_t* const payload, int32_t payload_size)) {
    ProcessMap.at(process) = handler;
}

uint32_t Protocol::ProtocolEngine::call_process(const std::vector<uint8_t>& payload) {
    // first byte as process id.
    if (payload.size() > sizeof(uint8_t)) {
        uint8_t process_id = payload.front();
        if (ProcessMap.at(process_id)) {
            return ProcessMap.at(process_id)(payload.data(), payload.size());
        }
    }
    LOG(std::cout << "Call function failed." << std::endl;)
    return 0;
}