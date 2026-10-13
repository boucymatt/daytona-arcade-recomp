// Exact byte access to little-endian packed texture memory on a big-endian CPU.
#pragma once
#include <cstdint>
namespace ps3 {
inline uint16_t texel_byte(uint32_t offset, int x, int y, const uint32_t *sheet) {
    const unsigned bit = ((offset & 1u) << 4) | ((1u ^ (unsigned(y) & 1u)) << 3)
                       | ((1u ^ (unsigned(x) & 1u)) << 2);
    const auto *bytes = reinterpret_cast<const unsigned char *>(sheet);
    return (bytes[((offset >> 1) & 0x7ffffu) * 4 + (bit >> 3)] >> (bit & 7)) & 15;
}
}
