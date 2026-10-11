#include "fp_cases.h"
#include <cstdio>
int main() {
    auto result = run_fp_cases();
    if (result.known_failures) return 1;
    std::printf("#define FP_EXPECTED_HASH 0x%08x%08xull\n#define FP_EXPECTED_CHECKS %uu\n",
        unsigned(result.hash >> 32), unsigned(result.hash), result.checks);
    return 0;
}
