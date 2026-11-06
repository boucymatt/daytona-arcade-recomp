#include "runtime/rom_import.h"

#include <atomic>
#include <string_view>

namespace rt {

#ifdef M2_MULTI_ROMSETS
namespace {
std::atomic<bool> revision_a{std::string_view(M2_ROMSET) == "daytona"};
}
#endif

const char *rom_set_name() {
#ifdef M2_MULTI_ROMSETS
    return revision_a.load(std::memory_order_relaxed) ? "daytona" : "daytona93";
#else
    return M2_ROMSET;
#endif
}

bool supports_rom_set(const std::string &romset) {
#ifdef M2_MULTI_ROMSETS
    return romset == "daytona93" || romset == "daytona";
#else
    return romset == M2_ROMSET;
#endif
}

bool select_rom_set(const std::string &romset) {
    if (!supports_rom_set(romset)) return false;
#ifdef M2_MULTI_ROMSETS
    revision_a.store(romset == "daytona", std::memory_order_relaxed);
#endif
    return true;
}

} // namespace rt
