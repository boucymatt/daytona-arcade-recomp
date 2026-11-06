// One mobile executable contains both statically recompiled games. The
// frontend destroys the old game before changing rt::rom_set_name().
#include "runtime/gen_support.h"
#include "runtime/rom_import.h"
#include "runtime/snd_gen_support.h"

#include <cstring>

namespace gen_daytona93 {
bool has_code(uint32_t);
void run(gen::Env &);
uint64_t native_instructions();
}
namespace gen_daytona {
bool has_code(uint32_t);
void run(gen::Env &);
uint64_t native_instructions();
}
namespace sndgen_daytona93 {
bool has_code(uint32_t);
void run(sndgen::Env &);
uint64_t native_instructions();
}
namespace sndgen_daytona {
bool has_code(uint32_t);
void run(sndgen::Env &);
uint64_t native_instructions();
}

namespace {
bool revision_a() { return std::strcmp(rt::rom_set_name(), "daytona") == 0; }
}

namespace gen {
bool has_code(uint32_t addr) {
    return revision_a() ? gen_daytona::has_code(addr) : gen_daytona93::has_code(addr);
}
void run(Env &env) {
    if (revision_a()) gen_daytona::run(env);
    else gen_daytona93::run(env);
}
uint64_t native_instructions() {
    return revision_a() ? gen_daytona::native_instructions() : gen_daytona93::native_instructions();
}
} // namespace gen

namespace sndgen {
bool has_code(uint32_t addr) {
    return revision_a() ? sndgen_daytona::has_code(addr) : sndgen_daytona93::has_code(addr);
}
void run(Env &env) {
    if (revision_a()) sndgen_daytona::run(env);
    else sndgen_daytona93::run(env);
}
uint64_t native_instructions() {
    return revision_a() ? sndgen_daytona::native_instructions() : sndgen_daytona93::native_instructions();
}
} // namespace sndgen
