#include "runtime/m2_board.h"
#include "runtime/guest_endian.h"
#include "tls_runtime.h"
#include <cstdio>
#include <memory>

#ifdef XBOX_BOARD_HOST_TEST
extern "C" int xbox_tls_init() { return 1; }
extern "C" void xbox_tls_thread_cleanup() {}
extern "C" int xbox_tls_shutdown() { return 1; }
static constexpr const char *path = "/tmp/daytona-xbox-board-host.log";
#else
static constexpr const char *path = "game:\\xbox360-board.log";
#endif
static bool log(const char *text) {
    FILE *file = fopen(path, "ab");
    if (!file) return false;
    bool ok = fprintf(file, "%s\n", text) >= 0;
    return fclose(file) == 0 && ok;
}
static void require(bool ok, const char *why) { if (!ok) throw rt::Fatal(why); }
static void put(std::vector<uint8_t> &bytes, size_t at) {
    // Literal byte oracle, independent of guest_store.
    bytes[at] = 0x78; bytes[at + 1] = 0x56; bytes[at + 2] = 0x34; bytes[at + 3] = 0x12;
}
static void test_board() {
    rt::M2Board::Images images;
    images.program.resize(0x200000);
    images.main_data.resize(0x2000000);
    images.copro_tables.resize(0x40000);
    images.copro_data.resize(0x800000);
    images.polygons.resize(16);
    images.textures.resize(16);
    put(images.program, 0); put(images.program, 0x20000);
    put(images.main_data, 0); put(images.main_data, 0x1000000);
    put(images.copro_data, 0);
    require(log("Synthetic images allocated; constructing real M2Board"), "logging");
    auto board = std::make_unique<rt::M2Board>(std::move(images));
    require(log("M2Board construction PASS"), "logging");
    for (uint32_t addr : {0u, 0x220000u, 0x2000000u, 0x6000000u}) {
        require(board->fetch(addr) == 0x12345678 && board->read_dword(addr) == 0x12345678 &&
                board->read_word(addr) == 0x5678 && board->read_word(addr + 2) == 0x1234 &&
                board->read_byte(addr) == 0x78, "ROM order or mirror");
        board->write_dword(addr, 0);
        require(board->read_dword(addr) == 0x12345678, "ROM write protection");
    }
    require(log("Program/main ROM fetch, byte order, mirrors, write protection PASS"), "logging");
    rt::Cpu cpu(board.get());
    unsigned cases = 0;
    for (uint32_t base : {0x200000u, 0x500000u, 0xe00000u, 0x1000000u, 0x1080000u,
                          0x1800000u, 0x1810000u, 0x1a00000u, 0x1d00000u, 0x11600000u, 0x12800000u}) {
        for (unsigned offset = 0; offset < 64; ++offset) {
            for (uint32_t value : {0u, 1u, 0x80000000u, 0x12345678u, 0xffffffffu}) {
                // CPU control RAM is only one page; other regions cross pages.
                const uint32_t addr = base + (base == 0xe00000u ? 0x100 : 0xfc0) + offset;
                cpu.i960_write_dword_unaligned(addr, value);
                require(cpu.i960_read_dword_unaligned(addr) == value, "RAM dword round trip");
                for (unsigned i = 0; i < 4; ++i)
                    require(board->read_byte(addr + i) == uint8_t(value >> (8 * i)), "RAM byte order");
                cpu.i960_write_word_unaligned(addr, uint16_t(value));
                require(cpu.i960_read_word_unaligned(addr) == uint16_t(value), "RAM word round trip");
                ++cases;
            }
        }
    }
    require(cases == 3520, "RAM case count");
    require(log("3520 RAM aligned/unaligned/page-crossing cases PASS"), "logging");
    board->write_dword(0x1000100, 0x89abcdef);
    require(board->read_dword(0x1110100) == 0x89abcdef, "tile RAM mirror");
    require(board->tgp().mem_r(0x800000) == 0x12345678, "TGP ROM word conversion");
    board->write_dword(0x900100, 0x12345678);
    board->write_byte(0x900101, 0xab);
    board->write_word(0x900102, 0xcdef);
    require(board->read_dword(0x940100) == 0xcdefab78, "TGP RAM masked lanes/mirror");
    board->write_dword(0x12000000, 0x12345678);
    require(board->read_word(0x12000000) == 0x5678 && board->read_word(0x12200000) == 0x5678,
            "texture aperture endian/mirror");
    board->write_dword(0x12000004, 0x89abcdef);
    require(board->read_dword(0x12000000) == 0xcdef5678, "texture packed lanes");
    require(log("Tile/TGP/texture RAM mirrors and masked lanes PASS"), "logging");
    // Device fetch must still fail rather than returning arbitrary memory.
    bool caught = false;
    try { (void)board->fetch(0x980000); } catch (const rt::Fatal &) { caught = true; }
    require(caught, "device fetch guard");
    require(log("Device fetch guard PASS; destroying board"), "logging");
}
int run_exception_cases();
int main() {
    FILE *file = fopen(path, "wb");
    if (!file) return 1;
    bool ok = fputs("Board probe START: synthetic data; TGP execution/link/scheduler guarded\n", file) >= 0;
    if (fclose(file) != 0 || !ok || !xbox_tls_init()) return 1;
    try { require(run_exception_cases() == 0, "exception regression"); test_board(); }
    catch (const std::exception &error) { log("Board probe FAIL:"); log(error.what()); ok = false; }
    catch (...) { log("Board probe FAIL: unknown exception"); ok = false; }
    xbox_tls_thread_cleanup();
    ok &= xbox_tls_shutdown() != 0;
    ok &= log(ok ? "Board probe complete: PASS" : "Board probe complete: FAIL");
    return ok ? 0 : 1;
}
