#include "runtime/gen_support.h"
#include "runtime/rom_import.h"
#include "runtime/snd_gen_support.h"

#include <cstdio>
#include <cstdlib>
#include <string>

// Distinct synthetic programs make dispatch mistakes visible without a ROM.
namespace gen_daytona93 {
bool has_code(uint32_t addr) { return addr == 93; }
void run(gen::Env &) {}
uint64_t native_instructions() { return 930; }
}
namespace gen_daytona {
bool has_code(uint32_t addr) { return addr == 94; }
void run(gen::Env &) {}
uint64_t native_instructions() { return 940; }
}
namespace sndgen_daytona93 {
bool has_code(uint32_t addr) { return addr == 193; }
void run(sndgen::Env &) {}
uint64_t native_instructions() { return 1930; }
}
namespace sndgen_daytona {
bool has_code(uint32_t addr) { return addr == 194; }
void run(sndgen::Env &) {}
uint64_t native_instructions() { return 1940; }
}

namespace {
void check(bool ok) { if (!ok) std::abort(); }
void check_program(bool revision_a) {
    check(std::string(rt::rom_set_name()) == (revision_a ? "daytona" : "daytona93"));
    check(gen::has_code(revision_a ? 94 : 93));
    check(!gen::has_code(revision_a ? 93 : 94));
    check(gen::native_instructions() == (revision_a ? 940 : 930));
    check(sndgen::has_code(revision_a ? 194 : 193));
    check(!sndgen::has_code(revision_a ? 193 : 194));
    check(sndgen::native_instructions() == (revision_a ? 1940 : 1930));
}
}

int main() {
    check(rt::supports_rom_set("daytona93") && rt::supports_rom_set("daytona"));
    check(!rt::supports_rom_set("daytonas") && !rt::supports_rom_set(""));
    check_program(false);
    check(rt::select_rom_set("daytona"));
    check_program(true);
    check(!rt::select_rom_set("daytonas"));
    check_program(true); // a rejected set cannot change running dispatch
    check(rt::select_rom_set("daytona93"));
    check_program(false);
    std::puts("Dual-ROM selection and i960/sound dispatch passed");
}
