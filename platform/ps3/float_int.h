#pragma once
#include <bit>
#include <cstdint>
#include <limits>
namespace ps3 {
// IEEE binary32 truncation with the runtime's INT32_MIN invalid sentinel.
inline int32_t truncate_coordinate(float f) {
    const uint32_t bits = std::bit_cast<uint32_t>(f);
    const unsigned exponent = (bits >> 23) & 255;
    if (exponent < 127) return 0;
    if (exponent >= 158) return std::numeric_limits<int32_t>::min();
    const uint32_t mantissa = (bits & 0x7fffff) | 0x800000;
    const uint32_t magnitude = exponent < 150 ? mantissa >> (150 - exponent)
                                             : mantissa << (exponent - 150);
    return bits >> 31 ? -int32_t(magnitude) : int32_t(magnitude);
}
}
