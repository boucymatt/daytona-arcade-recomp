#pragma once

#include "app/config.h"
#include "runtime/rom_import.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <fstream>
#include <string>

namespace app {

// The selected version is shared; each version keeps its own launcher.ini
// and cabinet saves in Config::pref_dir(). Existing installs start at Deluxe.
inline std::string rom_selection_file() {
    const std::string name = Config::profile.empty() ? "launcher" : "launcher-" + Config::profile;
    char *base = SDL_GetPrefPath("daytona-recomp", name.c_str());
    if (!base) return {};
    const std::string path = std::string(base) + "romset.txt";
    SDL_free(base);
    return path;
}

inline void restore_rom_selection() {
    const std::string path = rom_selection_file();
    if (path.empty()) return;
    std::ifstream input(path);
    std::string romset;
    if (std::getline(input, romset)) rt::select_rom_set(romset);
}

inline void save_rom_selection() {
    const std::string path = rom_selection_file();
    if (!path.empty()) std::ofstream(path) << rt::rom_set_name() << '\n';
}

} // namespace app
