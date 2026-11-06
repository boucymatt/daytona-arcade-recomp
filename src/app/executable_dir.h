#pragma once

#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <vector>
#else
#include <SDL3/SDL_filesystem.h>
#endif

namespace app {

// Game executables live beside the launcher, including inside a macOS
// bundle's Contents/MacOS. SDL_GetBasePath returns Contents/Resources there.
inline std::filesystem::path executable_dir() {
#ifdef __APPLE__
    uint32_t size = 1024;
    std::vector<char> path(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        path.resize(size);
        if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
    }
    std::error_code ec;
    const auto executable = std::filesystem::canonical(path.data(), ec);
    return ec ? std::filesystem::path{} : executable.parent_path();
#else
    const char *base = SDL_GetBasePath();
    return base ? std::filesystem::path(base) : std::filesystem::path{};
#endif
}

} // namespace app
