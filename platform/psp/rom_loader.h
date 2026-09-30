#pragma once
#include "runtime/game_loop.h"
#include "runtime/native_sound_engine.h"
#include <memory>
#include <string>

namespace psp {
struct LoadedGame {
    rt::M2Board::Images images;
    std::unique_ptr<snd::NativeSoundEngine> audio;
};
// Uses the host importer's verified, full-size region files, never a zip
// expanded in PSP RAM. Main-board and audio caches have separate owners.
LoadedGame load_game(const std::string& directory);
}
