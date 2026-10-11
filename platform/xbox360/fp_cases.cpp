#include "fp_cases.h"
#include "i960/fp.h"
using namespace i960::fp;

FpReport run_fp_cases() {
    FpReport report{14695981039346656037ull, 0, 0};
    auto add = [&](Result result) {
        // Hash fields in fixed byte order, never ABI padding or host layout.
        for (unsigned i = 0; i < 5; ++i) {
            report.hash ^= i == 4 ? result.flags : uint8_t(result.value >> (i * 8));
            report.hash *= 1099511628211ull;
        }
        ++report.checks;
    };
    auto known = [&](Result result, uint32_t value, uint8_t flags) {
        if (result.value != value || result.flags != flags) ++report.known_failures;
        add(result);
    };
    known(ref_cvtri(0x3fc00000, Round::Nearest), 2, kInexact);
    known(ref_cvtri(0x40200000, Round::Nearest), 2, kInexact);
    known(ref_cvtri(0xbfc00000, Round::Down), 0xfffffffe, kInexact);
    known(ref_cvtri(0xbfc00000, Round::Up), 0xffffffff, kInexact);
    known(ref_cvtzri(0xbfc00000), 0xffffffff, kInexact);
    known(ref_cvtir(0xfffffffe, Round::Nearest), 0xc0000000, 0);
    known(ref_scaler(1, 0x3f800000, Round::Nearest), 0x40000000, 0);
    known(ref_cmpr(0x80000000, 0), kEqual, 0);
    known(ref_cvtri(0x7f800000, Round::Nearest), 0x80000000, kInvalid);
    constexpr uint32_t special[] = {0, 0x80000000, 1, 0x80000001, 0x007fffff,
        0x00800000, 0x3f000000, 0x3fc00000, 0x40200000, 0x4effffff, 0x4f000000,
        0xcf000000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000,
        0x7fc00000, 0x7f800001, 0xffc12345};
    uint32_t state = 0x12345678;
    auto random = [&]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned i = 0; i < 4096; ++i) {
        uint32_t a = i < sizeof(special) / sizeof(*special) ? special[i] : random();
        uint32_t b = random();
        int32_t exponent = int32_t(i % 1001) - 500;
        add(ref_cvtzri(a));
        add(ref_cmpr(a, b));
        for (unsigned mode = 0; mode < 4; ++mode) {
            add(ref_cvtri(a, Round(mode)));
            add(ref_cvtir(a, Round(mode)));
            add(ref_scaler(uint32_t(exponent), a, Round(mode)));
        }
    }
    return report;
}
