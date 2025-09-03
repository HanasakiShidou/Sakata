#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// C level API for Sakata

struct CMemoryReference{
    const void* ptr  = NULL;
    size_t len = 0;
};

typedef void (*CMemoryHandler) (struct CMemoryReference);

typedef void (*CMemoryHandlerRx) (struct CMemoryReference*, int instanceFd, int peerFd);

#ifdef __cplusplus
extern "C" {
#endif

// Initalize a instance, returns a FD.
int getInstance();

// Register a peer fd.
int registerPeer(int fd, CMemoryHandler on_tx_data);

// Checker if a funcuion exists on server
int registerFunction(const char* const functionDescriptor);

// Synchronous/Asynchronous call function (returns token).
int callFunction(struct CMemoryReference income, struct CMemoryReference* outcome, int32_t func_id, bool wait=true);

// Get result by token.
//int getResult(int token, struct CMemoryReference* outcome);

// Synchronous call simple function (returns result).
uint64_t fast_call(uint64_t income, int32_t func_id, bool wait=true);

/*
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
*/

#ifdef __cplusplus
}
#endif
