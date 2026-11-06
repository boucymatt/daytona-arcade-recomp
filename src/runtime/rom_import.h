// Load the user's ROM set (a MAME-format zip or 7z) into the memory images
// the board runs from, in memory: every file CRC-checked against MAME's
// ROM_START for the selected set (daytona93, the default, or daytona,
// Revision A), then laid out as MAME
// loads it (ROM_LOAD32_WORD interleave, ROM_COPY mirrors). The build's
// importer (tools/m2import) uses the same tables; scripts/m2import.py is the
// zip-only daytona93 reference they were checked against.
#pragma once

#include "runtime/m2_board.h"

// The initial ROM set (CMake's M2_ROMSET); single-set builds support only
// this set. Mobile builds can include both through M2_MULTI_ROMSETS.
#ifndef M2_ROMSET
#define M2_ROMSET "daytona93"
#endif

#include <string>
#include <vector>

namespace rt {

struct RomCheck {
    std::string file;
    bool ok = false;
    std::string problem; // missing, wrong size, wrong CRC
};

// Check a zip without loading it (for the launcher's status line).
std::vector<RomCheck> check_rom_set(const std::string &zip_path);
// Validate a candidate without changing the running game's ROM set.
std::vector<RomCheck> check_rom_set(const std::string &zip_path, const std::string &romset);

// Load and lay out the images. Throws ZipError naming the first problem.
M2Board::Images import_rom_set(const std::string &zip_path);

// The active set. Only switch after destroying the old game and joining its
// audio work; generated i960 and sound entry points follow this selection.
const char *rom_set_name();
bool supports_rom_set(const std::string &romset);
// Unsupported names leave the current selection unchanged.
bool select_rom_set(const std::string &romset);

// The MAME names of the sets an archive holds in full (every file there, at
// its size and CRC), this build's own first: an archive can hold both (MAME's
// merged daytona). For the launcher, whose other set is the other game's.
std::vector<std::string> rom_sets_in(const std::string &zip_path);

// The TGP program the i960 uploads at boot, cut from main_data (CRC-checked).
std::vector<uint8_t> tgp_program(const M2Board::Images &img);

} // namespace rt
