#include "fp_cases.h"
#include "fp_expected.h"
#include "tls_runtime.h"
#include <cstddef>
#include <cstdio>
extern "C" {
#include "softfloat.h"
}
static_assert(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
static_assert(offsetof(extFloat80_t, signExp) == 0);
static_assert(offsetof(extFloat80_t, signif) > offsetof(extFloat80_t, signExp));
int run_exception_cases();
static bool log(const char *message) {
    FILE *file = fopen("game:\\xbox360-fp.log", "ab");
    if (!file) return false;
    bool ok = fputs(message, file) >= 0;
    return fclose(file) == 0 && ok;
}
int main() {
    FILE *file = fopen("game:\\xbox360-fp.log", "wb");
    if (!file) return 1;
    bool ok = fputs("SoftFloat probe START: big-endian, synthetic data, no game\n", file) >= 0;
    if (fclose(file) != 0 || !ok) return 1;
    if (!xbox_tls_init()) { log("TLS init FAIL\n"); return 1; }
    ok = run_exception_cases() == 0;
    ok &= log(ok ? "Exception regression PASS\n" : "Exception regression FAIL\n");
    // Also cross-check the C-returned aggregate layout against named fields.
    auto one = f32_to_extF80(float32_t{0x3f800000});
    ok &= one.signExp == 0x3fff && one.signif == 0x8000000000000000ull;
    ok &= log(ok ? "extF80 aggregate ABI PASS\n" : "extF80 aggregate ABI FAIL\n");
    auto result = run_fp_cases();
    char line[256];
    snprintf(line, sizeof(line), "Reference checks=%u known_failures=%u hash=%08x%08x expected=%08x%08x\n",
        result.checks, result.known_failures, unsigned(result.hash >> 32), unsigned(result.hash),
        unsigned(FP_EXPECTED_HASH >> 32), unsigned(FP_EXPECTED_HASH));
    ok &= log(line);
    ok &= result.hash == FP_EXPECTED_HASH && result.checks == FP_EXPECTED_CHECKS && !result.known_failures;
    xbox_tls_thread_cleanup();
    ok &= xbox_tls_shutdown() != 0;
    if (!log(ok ? "SoftFloat probe complete: PASS\n" : "SoftFloat probe complete: FAIL\n")) return 1;
    return ok ? 0 : 1;
}
