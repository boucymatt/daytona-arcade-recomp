#include "float_int.h"
#include <cstdio>
#include <initializer_list>
static bool check(uint32_t bits) {
    const float f = std::bit_cast<float>(bits);
    const int32_t reference = f >= -2147483648.0f && f < 2147483648.0f
        ? int32_t(f) : std::numeric_limits<int32_t>::min();
    return ps3::truncate_coordinate(f) == reference;
}
int main() {
    // Every exponent/sign, mantissa edges, plus four million random bit patterns.
    for (uint32_t e = 0; e < 512; ++e)
        for (uint32_t m : {0u, 1u, 0x3fffffu, 0x400000u, 0x7ffffeu, 0x7fffffu})
            if (!check((e << 23) | m)) return 1;
    uint32_t state = 0x12345678;
    for (unsigned i = 0; i < 4000000; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        if (!check(state)) return 1;
    }
    std::puts("PASS: 4,003,072 float-to-int comparisons");
}
