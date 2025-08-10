#include <vector>
#include <cstdint>
#include <functional>
#include <iostream>
#include <cstring>
#include <span>

#define LOG_ENABLED 1

#define LOG(x) \
    if(LOG_ENABLED) { \
        x             \
    }

// 协议字段定义
namespace Protocol {
    constexpr uint8_t START_FLAG       = 0xAA;
    constexpr uint8_t END_FLAG         = 0x55;
    constexpr size_t  MAX_PAYLOAD      = 256;  // 最大有效载荷
    constexpr size_t  MAX_BUFFERSIZE   = 1024;
    
    enum class CmdType : uint8_t {
        REQUEST,    // 请求
        RESPONSE,   // 响应
        RETRY,      // 重传请求
        BUSY,       // 繁忙状态
        FREE        // 空闲状态
    };

#pragma pack(push, 1)
    struct FrameHeader {
        uint8_t     startFlag;     // 起始标志
        CmdType     cmd;           // 命令类型
        uint16_t    frameId;       // 帧序号（允许溢出归零）
        uint16_t    dataLength;    // 数据长度
        // 数据载荷紧随其后
        // XOR校验值在数据之后
        // 结束标志在最后
    };
#pragma pack(pop)

    class ProtocolEngine {
    public:
        using FrameHandler = std::function<const std::vector<uint8_t>(const std::vector<uint8_t>&, uint16_t)>;
        
        uint16_t current_frame_id{0};

        static ProtocolEngine inst;

        static ProtocolEngine& instance() {
            return inst;
        }

        static uint32_t call_process(const std::vector<uint8_t>& payload);
        
        // 接收数据入口
        void process_rx_data(const uint8_t* data, size_t len) {
            LOG(std::cout << "on data recv " << len << " bytes " << std::endl;)
            if (len != 0) {
                rx_buffer.insert(rx_buffer.end(), data, data + len);
            }
            try_parse_frames();
            LOG(std::cout << "on data recv process end." << std::endl << std::endl;)
        }

        // 构建发送帧
        static std::vector<uint8_t> buildFrame(
            Protocol::CmdType cmd,
            uint16_t frameId,
            const std::vector<uint8_t>& payload)
        {
            std::vector<uint8_t> frame;
            frame.reserve(sizeof(Protocol::FrameHeader) + payload.size() + 2);
            
            // 头部
            Protocol::FrameHeader header;
            header.startFlag  = Protocol::START_FLAG;
            header.cmd        = cmd;
            header.frameId    = frameId;
            header.dataLength = static_cast<uint16_t>(payload.size());
            
            // 添加头部
            auto headerPtr = reinterpret_cast<uint8_t*>(&header);
            frame.insert(frame.end(), headerPtr, headerPtr + sizeof(header));
            
            // 添加载荷
            if (payload.size() != 0) {
                frame.insert(frame.end(), payload.begin(), payload.end());
            }
            
            // 计算校验（从cmd字段开始到数据结束）
            uint8_t checksum = 0;
            for (size_t i = 1; i < frame.size(); ++i) {
                checksum ^= frame[i];
            }
            frame.push_back(checksum);
            
            // 结束标志
            frame.push_back(Protocol::END_FLAG);
            
            return frame;
        }

        // 解析接收帧（返回空帧表示解析失败）
        static bool parseFrame(
            const std::vector<uint8_t>& rawData,
            Protocol::CmdType& cmd,
            uint16_t& frameId,
            std::vector<uint8_t>& payload)
        {
            // 查找起始和结束标志
            auto startIt = std::find(rawData.begin(), rawData.end(), Protocol::START_FLAG);
            auto endIt = std::find(rawData.rbegin(), rawData.rend(), Protocol::END_FLAG).base();
            
            if (startIt == rawData.end() || endIt == rawData.begin()) {
                return false;
            }
            
            // 提取完整帧
            //std::vector<uint8_t> frame(startIt, endIt);
            const std::vector<uint8_t>&frame = rawData;
            
            // 验证最小长度
            if (frame.size() < sizeof(Protocol::FrameHeader) + 2) { // 头+校验+结束
                return false;
            }
            
            // 校验结束标志
            if (*(endIt - 1) != Protocol::END_FLAG) {
                return false;
            }
            
            // 验证校验和
            uint8_t checksum = 0;
            for (size_t i = 1; i < frame.size() - 2; ++i) { // 排除起始和校验、结束
                checksum ^= frame[i];
            }
            if (checksum != frame[frame.size() - 2]) {
                return false;
            }
            
            // 解析头部
            Protocol::FrameHeader header;
            memcpy(&header, frame.data(), sizeof(header));
            
            // 验证数据长度
            size_t expectedSize = sizeof(header) + header.dataLength + 2; // +校验+结束
            if (frame.size() != expectedSize) {
                return false;
            }
            
            // 提取有效载荷
            payload = std::vector<uint8_t>(
                frame.begin() + sizeof(header),
                frame.begin() + sizeof(header) + header.dataLength
            );
            
            cmd = header.cmd;
            frameId = header.frameId;
            return true;
        }
        
        void handleFrame(const Protocol::CmdType cmd, const uint16_t frameId, const std::vector<uint8_t>& payload) {


        }

        void send_response(const std::vector<uint8_t> payload, uint16_t frame_id) {
            send_raw_frame(buildFrame(CmdType::RESPONSE, frame_id, payload));
        }

        void send_control_info(CmdType cmd, uint16_t frame_id) {
            send_raw_frame(buildFrame(cmd, frame_id, {}));
        }

        // 设置服务端回调
        void set_server_handler(FrameHandler handler) { server_handler = handler; }

        // 设置客户端回调 
        void set_client_handler(FrameHandler handler) { client_handler = handler; }

        using SendDataHandler = std::function<void(const std::vector<uint8_t>&, uint16_t)>;
        void set_send_callback(SendDataHandler handler) { on_cdc_send = handler; }

        // 发送原始帧
        void send_raw_frame(const std::vector<uint8_t>& frame) {
            if (on_cdc_send) {
                on_cdc_send(frame, 0);
            }
        }

    private:
        ProtocolEngine() = default;

        void try_parse_frames() {
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

        void dispatch_frame(const std::vector<uint8_t>& frame) {            
            CmdType cmd;
            uint16_t frameId;
            std::vector<uint8_t> payload;
            if (parseFrame(frame, cmd, frameId, payload)) {
                LOG(std::cout << "Recv frame " << frame.size() << " bytes" << std::endl;)
                LOG(std::cout << "Recv payload " << payload.size() << " bytes" << std::endl;)

                switch (cmd)
                {
                case CmdType::REQUEST:
                    if(payload.size() > 0 && server_handler) {
                        auto response = server_handler(payload, payload.size());
                        send_response(response, frameId);
                    }
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

        // 协议常量
        static constexpr uint8_t START_FLAG = 0xAA;
        static constexpr uint8_t END_FLAG = 0x55;

        std::vector<uint8_t> rx_buffer;
        FrameHandler server_handler;
        FrameHandler client_handler;
        SendDataHandler on_cdc_send;
    };

} // namespace Protocol
