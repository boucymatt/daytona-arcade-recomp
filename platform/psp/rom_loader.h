#pragma once
#include "runtime/game_loop.h"
#include "runtime/native_sound_engine.h"
#include "runtime/paged_rom.h"
#include <array>
#include <memory>
#include <string>

namespace psp {
struct LoadedGame {
    rt::M2Board::Images images;
    std::unique_ptr<snd::NativeSoundEngine> audio;
};
// Uses the host importer's verified, full-size region files, never a zip
// expanded in PSP RAM. Main-board and audio caches have separate owners.
inline constexpr std::array<const char*, 7> kRomNames{
    "program", "main_data", "copro_data", "polygons", "textures", "pcm1", "pcm2"};
LoadedGame load_game(const std::string& directory, rt::PagedRom::IoObserver observer = nullptr,
                     const std::array<void*, 7>& contexts = {});
}
