#pragma once

#include <cstdint>

struct BinaryMessageHeader {
    uint8_t message_type;   // Corresponds to MessageType enum
    uint32_t metadata_size; // Size of metadata JSON
    // Followed by:
    // 1. Metadata (JSON bytes)
    // 2. File data (binary)
};