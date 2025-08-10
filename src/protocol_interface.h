#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化协议栈
void protocol_init(void (*callback)(const void*, uint32_t));

// 接收数据处理入口（由CDC回调触发）
void protocol_on_rx_data(const uint8_t* data, uint32_t len);

    // 发送请求（客户端使用）
void protocol_send_request(const uint8_t* data, uint32_t len);

// 发送响应（服务端使用）
//void protocol_send_response(const uint8_t* data, size_t len);

// Function registration

enum ProcessList{
    DUMMY,
    ALL_PROECESS
};

void register_function(enum ProcessList process, uint32_t (*handler)(const uint8_t* const payload, int32_t payload_size));

// PARAMS = int val, int val2
#define UNPACK_PARAMS(type, var_name)                           \
    type var_name;                                              \
    memset(&var_name, 0, sizeof(var_name));                     \
    memcpy(&var_name, payload + offset, sizeof(var_name));      \
    offset += sizeof(var_name);                                 \


#define DEFINE_PROCESS_1PARAM(FUNC_NAME, FUNC_ID, PARAMS_TYPE1)                             \
uint32_t TEMP_PROCESS_##FUNC_NAME(const uint8_t* const payload, int32_t payload_size) {     \
    int offset = 1;                                                                         \
    UNPACK_PARAMS(PARAMS_TYPE1, PARAM1)                                                     \
    return FUNC_NAME(PARAM1);                                                               \
}                                                

#define REGISTER_PROCESS_1PARAM(FUNC_NAME, FUNC_ID) do {    \
    register_function(FUNC_ID, TEMP_PROCESS_##FUNC_NAME);   \
} while (0)

#define CALL_1PARAM(FUNC_ID, PARAM1) do { \
    uint8_t send_buff[1 + sizeof(PARAM1)]; \
    int offset = 1; \
    memset(send_buff, 0, sizeof(send_buff)); \
    memcpy(&send_buff[offset], &PARAM1, sizeof(PARAM1)); \
    protocol_send_request(send_buff, 1 + sizeof(PARAM1)); \
} while (0) \


#ifdef __cplusplus
}
#endif
