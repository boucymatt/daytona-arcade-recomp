#include "rom_loader.h"
#include "runtime/paged_rom.h"
#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>

namespace psp {
namespace {
std::vector<uint8_t> resident(const std::string& path, size_t expected) {
    std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(), "rb"), std::fclose);
    if (!file) throw std::runtime_error("missing " + path + "; run prepare_psp.py first");
    if (std::fseek(file.get(), 0, SEEK_END) || std::ftell(file.get()) != long(expected) ||
        std::fseek(file.get(), 0, SEEK_SET))
        throw std::runtime_error("incorrect ROM region size: " + path);
    std::vector<uint8_t> bytes(expected);
    if (std::fread(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
        throw std::runtime_error("ROM read failed: " + path);
    return bytes;
}
}
LoadedGame load_game(const std::string& directory, rt::PagedRom::IoObserver observer,
                     const std::array<void*, 7>& contexts) {
    const auto path = [&](const char* name) { return directory + "/" + name + ".bin"; };
    size_t region = 0;
    const auto paged = [&](const char* name, uint32_t bytes, size_t cache) {
        auto image = std::make_shared<rt::PagedRom>(path(name), bytes, cache);
        image->set_io_observer(observer, contexts.at(region++));
        return image;
    };
    LoadedGame game;
    // Fixed 832 KiB main-board cache budget; logical ROM masks stay intact.
    game.images.program_file = paged("program", 0x200000, 0x40000);
    game.images.main_data_file = paged("main_data", 0x2000000, 0x20000);
    game.images.copro_data_file = paged("copro_data", 0x800000, 0x10000);
    game.images.polygons_file = paged("polygons", 0x1000000, 0x40000);
    game.images.textures_file = paged("textures", 0x1000000, 0x20000);
    game.images.copro_tables = resident(path("copro_tables"), 0x40000);
    // These two 256 KiB caches are exclusively used by the audio thread.
    auto pcm1 = paged("pcm1", 0x400000, 0x40000);
    auto pcm2 = paged("pcm2", 0x400000, 0x40000);
    game.audio = std::make_unique<snd::NativeSoundEngine>(
        resident(path("sound_program"), 0x40000), std::move(pcm1), std::move(pcm2));
    return game;
}
}
