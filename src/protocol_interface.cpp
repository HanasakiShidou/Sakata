#include "protocol_engine.hpp"
#include "protocol_interface.h"

#include <cassert>
#include <vector>

CMemoryReference referenceWrapper(Protocol::MemoryReference ref) {
    return {
        .ptr = reinterpret_cast<const void*>(ref.data()),
        .len = ref.size(),
    };
}

Protocol::MemoryReference cReferenceWrapper(CMemoryReference ref) {
    return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(ref.ptr), ref.len);
}

std::vector<Protocol::ProtocolEngine> instances;

#ifdef __cplusplus
extern "C" {
#endif

int getInstance() {
    instances.push_back(Protocol::ProtocolEngine());
    return instances.size() - 1;
}

int registerPointToPoint(int fd, CMemoryHandler on_tx_data) {
    if (fd >= 0 && fd < instances.size()) {
        instances.at(fd).set_send_callback([on_tx_data = on_tx_data](Protocol::MemoryReference memoryReference) {
            return on_tx_data(referenceWrapper(memoryReference));
        });
        return 0;
    } else {
        return -1;
    }
}

int onPointToPointRecive(int instance_fd, int ptp_fd, CMemoryReference memRef) {
    instances.at(instance_fd).onPointToPointRecive(ptp_fd, cReferenceWrapper(memRef));
    return 0;
}

int registerFunction(const char* const functionDescriptor) {

    return 0;
}

int callFunction(struct CMemoryReference income, struct CMemoryReference* outcome, int32_t func_id, bool wait) {
    // read parameter
    assert(income.ptr);
    assert(income.len != 0);
    assert(wait);

    return -1;
}

uint64_t fast_call(uint64_t income, int32_t func_id, bool wait) {
    assert(wait);
    return 0;

}

#ifdef __cplusplus
}
#endif