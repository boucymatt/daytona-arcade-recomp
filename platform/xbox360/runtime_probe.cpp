// Real runtime services with synthetic data, not a ROM or interpreter.
#include "runtime/cpu.h"
#include "tls_runtime.h"
#include <cstring>
#include <vector>

#ifdef XBOX_RUNTIME_HOST_TEST
// Host verifies the synthetic test expectations, not the kernel TLS adapter.
extern "C" int xbox_tls_init() { return 1; }
extern "C" void xbox_tls_thread_cleanup() {}
extern "C" int xbox_tls_shutdown() { return 1; }
static constexpr const char *log_path = "/tmp/daytona-xbox-runtime-host.log";
#else
static constexpr const char *log_path = "game:\\xbox360-runtime.log";
#endif

namespace {
thread_local unsigned samples;
class Memory final : public rt::Bus {
public:
    std::vector<uint8_t> bytes = std::vector<uint8_t>(65536);
    uint32_t fetch(uint32_t a) override { return read_dword(a); }
    uint8_t read_byte(uint32_t a) override { return bytes.at(a); }
    uint16_t read_word(uint32_t a) override {
        return uint16_t(read_byte(a)) | uint16_t(read_byte(a + 1)) << 8;
    }
    uint32_t read_dword(uint32_t a) override {
        return uint32_t(read_word(a)) | uint32_t(read_word(a + 2)) << 16;
    }
    void write_byte(uint32_t a, uint8_t v) override { bytes.at(a) = v; }
    void write_word(uint32_t a, uint16_t v) override {
        write_byte(a, uint8_t(v)); write_byte(a + 1, uint8_t(v >> 8));
    }
    void write_dword(uint32_t a, uint32_t v) override {
        write_word(a, uint16_t(v)); write_word(a + 2, uint16_t(v >> 16));
    }
    uint16_t flags(uint32_t) override { return rt::Cpu::BURST; }
};

bool record(const char *text) {
    FILE *file = fopen(log_path, "ab");
    if (!file) return false;
    bool ok = fprintf(file, "%s\n", text) >= 0;
    return fclose(file) == 0 && ok;
}
void require(bool value, const char *message) {
    if (!value) throw rt::Fatal(message);
}

void check_runtime() {
    Memory memory;
    rt::Cpu cpu(&memory);
    memory.write_dword(0, 0x100);
    memory.write_dword(4, 0x200);
    memory.write_dword(12, 0x12345678);
    memory.write_dword(0x218, 0x1000);
    cpu.reset();
    require(cpu.m_SAT == 0x100 && cpu.m_PRCB == 0x200 && cpu.m_IP == 0x12345678 &&
            cpu.m_r[rt::I960_FP] == 0x1000 && cpu.m_r[rt::I960_SP] == 0x1040 &&
            cpu.m_PC == 0x001f2002 && cpu.m_rcache_pos == 0, "reset state");
    require(record("Runtime reset: PASS"), "log reset");

    unsigned checks = 0;
    for (uint32_t offset = 0; offset < 64; ++offset) {
        for (uint32_t value : {0u, 1u, 0x80000000u, 0x12345678u, 0xffffffffu}) {
            uint32_t address = 0x300 + offset;
            cpu.i960_write_dword_unaligned(address, value);
            require(cpu.i960_read_dword_unaligned(address) == value, "dword access");
            for (unsigned b = 0; b < 4; ++b)
                require(memory.bytes[address + b] == uint8_t(value >> (b * 8)), "guest byte order");
            auto read = cpu.i960_read_dword_unaligned_flags(address);
            require(read.first == value && read.second == rt::Cpu::BURST, "burst flags");
            cpu.i960_write_word_unaligned(address, uint16_t(value));
            require(cpu.i960_read_word_unaligned(address) == uint16_t(value), "word access");
            ++samples;
            ++checks;
        }
    }
    require(samples == checks && checks == 320, "compiler TLS integration");
    require(record("320 aligned/unaligned memory, byte-order, flags and TLS cases: PASS"), "log memory");

    for (unsigned depth = 1; depth <= 12; ++depth) {
        cpu.reset();
        for (unsigned i = 0; i < depth; ++i) {
            cpu.m_r[3] = 0xa5000000u + i;
            cpu.m_IP = 0x4000 + i * 4;
            cpu.do_call(0x8000 + i * 4, 0, 0);
            require(cpu.m_rcache_pos == int(i + 1), "call cache position");
        }
        for (unsigned i = depth; i-- > 0;) {
            cpu.do_ret_0();
            require(cpu.m_r[3] == 0xa5000000u + i && cpu.m_IP == 0x4000 + i * 4 &&
                    cpu.m_rcache_pos == int(i), "register cache restore");
        }
        require(cpu.m_r[rt::I960_FP] == 0x1000 && cpu.m_r[rt::I960_SP] == 0x1040, "stack restore");
    }
    require(record("Call/return depths 1..12, including register spills: PASS"), "log calls");
    bool caught = false;
    try { rt::fatalerror("runtime fault %u", 73u); }
    catch (const rt::Fatal &error) { caught = std::strcmp(error.what(), "runtime fault 73") == 0; }
    require(caught, "runtime exception message");
    caught = false;
    try { memory.bytes.at(memory.bytes.size()) = 1; }
    catch (const std::out_of_range &) { caught = true; }
    require(caught, "libc++ exception");
    require(record("Runtime Fatal and libc++ out_of_range exceptions: PASS"), "log exceptions");
}
}

int run_exception_cases();
int main() {
    FILE *file = fopen(log_path, "wb");
    if (!file) return 1;
    bool start = fputs("Runtime probe START: synthetic data, no game or graphics\n", file) >= 0;
    if (fclose(file) != 0 || !start) return 1;
    if (!xbox_tls_init()) { record("TLS init FAIL"); return 1; }
    int result = 0;
    try {
        require(run_exception_cases() == 0, "exception regression");
        require(record("Exception regression: PASS"), "log exception regression");
        check_runtime();
    } catch (const std::exception &error) {
        record("Runtime probe FAIL:"); record(error.what()); result = 1;
    } catch (...) { record("Runtime probe FAIL: unknown exception"); result = 1; }
    xbox_tls_thread_cleanup();
    if (!xbox_tls_shutdown()) { record("TLS shutdown FAIL"); result = 1; }
    if (!record(result ? "Runtime probe complete: FAIL" : "Runtime probe complete: PASS")) result = 1;
    return result;
}
