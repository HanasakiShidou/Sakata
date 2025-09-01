#include "protocol_engine.hpp"

// 协议字段定义
namespace Protocol {

    ProtocolEngine ProtocolEngine::inst;

    void ProtocolEngine::dispatch_frame(const std::vector<uint8_t>& frame) { 
        CmdType cmd;
        uint16_t frameId;
        std::vector<uint8_t> payload;
        if (parseFrame(frame, cmd, frameId, payload)) {
            LOG(std::cout << "Recv frame " << frame.size() << " bytes" << std::endl;)
            LOG(std::cout << "Recv payload " << payload.size() << " bytes" << std::endl;)

            switch(cmd) {
                case CmdType::REQUEST:
                    if(payload.size() > 0 && server_handler) {
                        auto response = server_handler(payload, payload.size());
                        send_response(response, frameId);
                    }
                    break;
                case CmdType::RESPONSE:
                    break;
                default:
                    break;
            }
        } else {
            LOG(std::cout << "Recv frame " << frame.size() << "bytes" << std::endl;)
            LOG(std::cout << "Recv payload " << payload.size() << "bytes" << std::endl;)
            LOG(std::cout << "Parse failed " << std::endl;)
            send_control_info(CmdType::RETRY, frameId);
        }
    }

    void ProtocolEngine::try_parse_frames() {
        while(true) {

            LOG(
            std::cout << "rx buffer:" << std::hex;
            for (auto ch: rx_buffer) {
                std::cout << " " << static_cast<int>(ch);
            }
            std::cout << std::dec <<std::endl;
            )

            auto start = rx_buffer.begin();
            for (; start != rx_buffer.end(); start++) {
                if (*start == START_FLAG)
                    break;
            }
            if(start == rx_buffer.end()) {
                LOG(std::cout << "no start flag, flush all." << std::endl;)
                rx_buffer.clear();
                break;
            }

            auto end = rx_buffer.end();
            for (; end != start; end--) {
                if (*end == END_FLAG)
                    break;
            }
            if(end <= start) {
                rx_buffer.erase(rx_buffer.begin(), start);
                LOG(std::cout << "parse failed, flushing bytes before start flag." << std::endl;)
                break;
            }

            LOG(std::cout << "Try parse " << rx_buffer.size() << " bytes " << std::endl;)

            std::vector<uint8_t> frame(start, end+1);
            rx_buffer.erase(rx_buffer.begin(), end+1);

            dispatch_frame(frame);

            if (rx_buffer.size() == 0) {
                break;
            }
        }
    }

} // namespace Protocol
