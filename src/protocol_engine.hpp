#include <vector>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <cstring>
#include <span>

#define LOG_ENABLED 1

#if LOG_ENABLED
#define LOG(x) \
    if(LOG_ENABLED) { \
        x             \
    }
#else
#define LOG(x) ;
#endif

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

    using MemoryReference = std::span<const uint8_t>;
    using MemoryReferenceHandler = std::function<void(std::span<const uint8_t>)>;

    class ProcessMap {
        public:
            int getProcessDescriptor(const std::string index);
            bool registerProcess(void(*callback)(struct Memory));
    };

    class PointToPointConnection {
        public:
            inline const bool isActive() { return active; }
            void initailze(MemoryReferenceHandler handler) { on_send = handler; active = true; }
            PointToPointConnection() = default;
            PointToPointConnection(MemoryReferenceHandler handler):on_send(handler), active(true) {}
        private:
            bool active{false};
            MemoryReferenceHandler on_send;
    };

    class ProtocolEngine {
    public:
        using FrameHandler = std::function<const std::vector<uint8_t>(const std::vector<uint8_t>&, uint16_t)>;
        
        uint16_t current_frame_id{0};

        static uint32_t call_process(const std::vector<uint8_t>& payload);

        // Point-to-Point connections.
        std::vector<PointToPointConnection> conntections;

        int registerPointToPoint(MemoryReferenceHandler handler) {
            conntections.push_back(PointToPointConnection(handler));
            return conntections.size() - 1;
        }

        int onPointToPointRecive(int ptp_fd, MemoryReference memRef);

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

        // 发送原始帧
        void send_raw_frame(const std::vector<uint8_t>& frame) {
            if (on_data_send) {
                on_data_send(frame);
            }
        }

        ProtocolEngine() = default;

    private:
        void try_parse_frames();
        void dispatch_frame(const std::vector<uint8_t>& frame);

        // 协议常量
        static constexpr uint8_t START_FLAG = 0xAA;
        static constexpr uint8_t END_FLAG = 0x55;

        std::vector<uint8_t> rx_buffer;
        MemoryReferenceHandler on_data_send;
    };

} // namespace Protocol
