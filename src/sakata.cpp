#include "sakata.hpp"

namespace Sakata
{

Packet Packet::deSerialization(const MemoryReference& data) {
    Packet packet;

    return packet;
}

const MemoryReference Packet::Serialization() {
    return MemoryReference(nullptr, 0);
}

}