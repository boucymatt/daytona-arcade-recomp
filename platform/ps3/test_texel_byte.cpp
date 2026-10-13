#include "texel_byte.h"
#include "runtime/guest_endian.h"
#include <vector>
#include <cstdio>
int main() {
    std::vector<uint32_t> sheet(0x80000);
    uint32_t state = 0x12345678;
    for (auto &v : sheet) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        rt::guest_store<uint32_t>(&v, state);
    }
    unsigned count = 0;
    for (uint32_t off = 0; off < 0x200000; ++off)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                uint32_t ref = rt::guest_load<uint32_t>(&sheet[(off >> 1) & 0x7ffff]);
                if (off & 1) ref >>= 16;
                if (!y) ref >>= 8;
                if (!x) ref >>= 4;
                if (ps3::texel_byte(off, x, y, sheet.data()) != (ref & 15)) return 1;
                ++count;
            }
    std::printf("PASS: %u byte texel comparisons including address wrapping\n", count);
}
