// Exercise the production config/selection persistence with app storage rooted
// in a temporary directory. No SDL initialisation or real user data is used.
#include "app/config.h"
#include "app/rom_selection.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
fs::path storage;
std::string active_set = "daytona93";
bool no_storage = false;

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

void write(const fs::path &path, const std::string &contents) {
    std::ofstream output(path);
    output << contents;
    require(bool(output), "cannot write test fixture");
}

std::string read(const fs::path &path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

// The runtime's generated-program selection is tested separately. This fake
// provides both supported choices so the persistence tests need no ROM code.
namespace rt {
const char *rom_set_name() { return active_set.c_str(); }
bool select_rom_set(const std::string &romset) {
    if (romset != "daytona93" && romset != "daytona") return false;
    active_set = romset;
    return true;
}
} // namespace rt

// The test target redirects SDL_GetPrefPath here, keeping the linked SDL
// library's own entry point intact (including its dynamic API table).
char *SDLCALL Test_GetPrefPath(const char *organization, const char *application) {
    require(std::strcmp(organization, "daytona-recomp") == 0, "unexpected preference organization");
    if (no_storage) return nullptr;
    const auto directory = storage / application;
    fs::create_directories(directory);
    const std::string path = directory.string() + "/";
    auto *copy = static_cast<char *>(SDL_malloc(path.size() + 1));
    if (copy) std::memcpy(copy, path.c_str(), path.size() + 1);
    return copy;
}

int main() {
    storage = fs::temp_directory_path() / ("daytona-rom-selection-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        ~Cleanup() { std::error_code ignored; fs::remove_all(storage, ignored); }
    } cleanup;
    try {
        app::Config::profile.clear();
        app::restore_rom_selection(); // first launch has no remembered version
        require(active_set == "daytona93", "first launch changed the initial version");
        const fs::path deluxe_directory = app::Config::pref_dir();
        require(fs::equivalent(deluxe_directory, storage / "daytona93"),
                "Deluxe preferences use the wrong directory");
        app::Config deluxe;
        deluxe.rom_path = "/imports/deluxe.zip";
        deluxe.volume = 0.25f;
        deluxe.save();
        write(deluxe_directory / "ioboard_eeprom.bin", "deluxe cabinet");
        app::save_rom_selection();

        rt::select_rom_set("daytona");
        const fs::path revision_directory = app::Config::pref_dir();
        require(revision_directory != deluxe_directory, "both games share cabinet saves");
        app::Config revision;
        revision.load();
        require(revision.rom_path.empty(), "Revision A inherited Deluxe's archive");
        revision.rom_path = "/imports/revision.7z";
        revision.volume = 0.75f;
        revision.save();
        write(revision_directory / "ioboard_eeprom.bin", "revision cabinet");
        app::save_rom_selection();
        rt::select_rom_set("daytona93");
        app::restore_rom_selection();
        require(active_set == "daytona", "remembered Revision A was not restored");
        app::Config reopened_revision;
        reopened_revision.load();
        require(reopened_revision.rom_path == revision.rom_path && reopened_revision.volume == 0.75f,
                "Revision A configuration did not survive switching");
        rt::select_rom_set("daytona93");
        app::Config reopened_deluxe;
        reopened_deluxe.load();
        require(reopened_deluxe.rom_path == deluxe.rom_path && reopened_deluxe.volume == 0.25f,
                "Revision A changed Deluxe's configuration");
        require(read(deluxe_directory / "ioboard_eeprom.bin") == "deluxe cabinet" &&
                read(revision_directory / "ioboard_eeprom.bin") == "revision cabinet",
                "cabinet saves changed across version switches");

        app::Config::profile = "2";
        const fs::path profile_directory = app::Config::pref_dir();
        require(fs::equivalent(profile_directory, storage / "daytona93-2"),
                "profile did not get its own game preferences");
        app::save_rom_selection();
        rt::select_rom_set("daytona");
        app::restore_rom_selection();
        require(active_set == "daytona93", "profile selection was not restored independently");
        app::Config::profile.clear();
        app::restore_rom_selection();
        require(active_set == "daytona", "profile selection overwrote the default selection");

        write(app::rom_selection_file(), "daytonas\n");
        app::restore_rom_selection();
        require(active_set == "daytona", "unsupported remembered version changed selection");
        write(app::rom_selection_file(), "");
        app::restore_rom_selection();
        require(active_set == "daytona", "empty selection file changed selection");
        no_storage = true;
        require(app::rom_selection_file().empty(), "failed storage lookup produced a relative path");
        app::save_rom_selection();
        app::restore_rom_selection();
        require(active_set == "daytona", "failed storage lookup changed selection");
        std::cout << "ROM selection, config and cabinet-save isolation passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "ROM selection test failed: " << error.what() << '\n';
        return 1;
    }
}
