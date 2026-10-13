#include "plane_math.h"
#include <bit>
#include <cstdio>
#include <cstdint>
#include <initializer_list>

int main() {
    unsigned checks = 0;
    for (int p = -65535; p <= 65535; ++p) {
        for (float sign : {-1.0f, 1.0f}) {
            const float plane = float(p);
            const float expected = std::hypot(sign, plane);
            const float actual = ps3::plane_hypot(sign, plane);
            for (float numerator : {sign, plane}) {
                if (std::bit_cast<uint32_t>(numerator / actual) !=
                    std::bit_cast<uint32_t>(numerator / expected)) return 1;
                ++checks;
            }
            if (std::bit_cast<uint32_t>(actual) != std::bit_cast<uint32_t>(expected)) return 2;
        }
    }
    std::printf("PASS: %u normalized components across signed 16-bit viewport range\n", checks);
}
