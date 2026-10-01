// Opt-in real-ROM audit. Compares dense and bounded file-backed native boards
// with the same generated code; no original code/assets are stored here.
#include "runtime/game_loop.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {
#ifdef M2_PSP_NATIVE_VIDEO
static_assert(rt::Video::OutputW == 480 && rt::Video::OutputH == 272);
static_assert(rt::GameLoop::kWidth == 480 && rt::GameLoop::kHeight == 272);
#else
static_assert(rt::Video::OutputW == 496 && rt::Video::OutputH == 384);
static_assert(rt::GameLoop::kWidth == 496 && rt::GameLoop::kHeight == 384);
#endif
unsigned number(const char *text) {
    errno = 0;
    char *end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (!*text || *text == '-' || *end || errno || value > std::numeric_limits<unsigned>::max())
        throw std::invalid_argument("invalid frame count");
    return unsigned(value);
}
std::vector<uint8_t> load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}
rt::M2Board::Images images(const std::string &directory, bool paged) {
    rt::M2Board::Images result;
    result.copro_tables = load(directory + "/copro_tables.bin");
    if (paged) {
        result.program_file = std::make_shared<rt::PagedRom>(directory + "/program.bin", 0x200000, 256 * 1024);
        result.main_data_file = std::make_shared<rt::PagedRom>(directory + "/main_data.bin", 0x2000000, 128 * 1024);
        result.copro_data_file = std::make_shared<rt::PagedRom>(directory + "/copro_data.bin", 0x800000, 64 * 1024);
        result.polygons_file = std::make_shared<rt::PagedRom>(directory + "/polygons.bin", 0x1000000, 256 * 1024);
        result.textures_file = std::make_shared<rt::PagedRom>(directory + "/textures.bin", 0x1000000, 128 * 1024);
    } else {
        result.program = load(directory + "/program.bin");
        result.main_data = load(directory + "/main_data.bin");
        result.copro_data = load(directory + "/copro_data.bin");
        result.polygons = load(directory + "/polygons.bin");
        result.textures = load(directory + "/textures.bin");
    }
    return result;
}
rt::Inputs input_for(unsigned frame, bool attract) {
    rt::Inputs in;
    if (attract) return in;
    if ((frame >= 1200 && frame < 1210) || (frame >= 1240 && frame < 1250) ||
        (frame >= 1280 && frame < 1290)) in.in0 &= ~1;
    if ((frame >= 1400 && frame < 1410) || (frame >= 1600 && frame < 1610) ||
        (frame >= 2000 && frame < 2010) || (frame >= 2400 && frame < 2410)) in.in0 &= ~0x10;
    if ((frame >= 1800 && frame < 1810) || (frame >= 2200 && frame < 2210)) in.in0 &= ~0x20;
    if (frame >= 1400) in.accel = 0xe0;
    return in;
}
void require(bool equal, const char *what, unsigned frame) {
    if (!equal) throw std::runtime_error(std::string(what) + " mismatch at frame " + std::to_string(frame));
}
bool equal_poly(const rt::GeoPoly &a, const rt::GeoPoly &b) {
    if (a.z != b.z || a.luma != b.luma || a.texlod != b.texlod || a.window != b.window ||
        a.reverse != b.reverse || a.num_vertices != b.num_vertices ||
        std::memcmp(a.texheader, b.texheader, sizeof a.texheader) ||
        std::memcmp(a.viewport, b.viewport, sizeof a.viewport) ||
        std::memcmp(a.center, b.center, sizeof a.center)) return false;
    for (unsigned i = 0; i < a.num_vertices; ++i) {
        if (std::bit_cast<uint32_t>(a.v[i].x) != std::bit_cast<uint32_t>(b.v[i].x) ||
            std::bit_cast<uint32_t>(a.v[i].y) != std::bit_cast<uint32_t>(b.v[i].y)) return false;
        for (unsigned p = 0; p < 3; ++p)
            if (std::bit_cast<uint32_t>(a.v[i].p[p]) != std::bit_cast<uint32_t>(b.v[i].p[p])) return false;
    }
    return true;
}
void compare_video(rt::Video &a, rt::Video &b, unsigned frame) {
    require(a.gpu_windows() == b.gpu_windows() && a.crtc_x() == b.crtc_x() && a.crtc_y() == b.crtc_y() &&
            a.render_x() == b.render_x() && a.render_y() == b.render_y(), "video state", frame);
    const auto &ap = a.gpu_polys(), &bp = b.gpu_polys();
    require(ap.size() == bp.size(), "polygon count", frame);
    for (size_t i = 0; i < ap.size(); ++i) require(equal_poly(ap[i], bp[i]), "polygon", frame);
    for (int layer = 0; layer < 4; ++layer) {
        require(!std::memcmp(a.system24_pixels(layer), b.system24_pixels(layer), 512 * 512 * 2), "tile pixels", frame);
        require(!std::memcmp(a.system24_flags(layer), b.system24_flags(layer), 512 * 512), "tile flags", frame);
    }
    const auto &am = a.gpu_mem(), &bm = b.gpu_mem();
    require(!std::memcmp(am.palram, bm.palram, 0x4000), "palette", frame);
    require(!std::memcmp(am.colorxlat, bm.colorxlat, 0xc000), "color translation", frame);
    require(!std::memcmp(am.lumaram, bm.lumaram, 0x20000), "luma", frame);
    require(!std::memcmp(am.tex0, bm.tex0, 0x200000) && !std::memcmp(am.tex1, bm.tex1, 0x200000), "texture RAM", frame);
}
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 6) {
        std::fprintf(stderr, "usage: test_psp_memory_rom ROM_DIR [FRAMES=6000] [race|attract] [RENDER_START=0] [RENDER_FRAMES=0]\n");
        return 2;
    }
    try {
        const unsigned count = argc > 2 ? number(argv[2]) : 6000;
        const std::string mode = argc > 3 ? argv[3] : "race";
        const unsigned render_start = argc > 4 ? number(argv[4]) : 0;
        const unsigned render_frames = argc > 5 ? number(argv[5]) : 0;
        if (!count || (mode != "race" && mode != "attract") ||
            render_start > count || render_frames > count - render_start)
            throw std::invalid_argument("invalid mode or render window");
        auto paged_images = images(argv[1], true);
        const std::array<std::shared_ptr<rt::PagedRom>, 5> files = {paged_images.program_file,
            paged_images.main_data_file, paged_images.copro_data_file, paged_images.polygons_file, paged_images.textures_file};
        rt::GameLoop dense(images(argv[1], false), false), paged(std::move(paged_images), false);
        const unsigned width = paged.board().video().output_width();
        const unsigned height = paged.board().video().output_height();
        require(width == rt::Video::OutputW && height == rt::Video::OutputH &&
                dense.board().video().output_width() == int(width) &&
                dense.board().video().output_height() == int(height), "output dimensions", 0);
        dense.board().video().set_external_3d(true);
        paged.board().video().set_external_3d(true);
        // Dense-ROM and file-backed reads must preserve every mapping,
        // mirror, burst flag and unmapped high address before game writes.
        // Both boards use the page-table implementation selected at compile
        // time (the PSP script selects sparse tables with M2_LOW_MEMORY).
        for (const auto &[first, bytes] : std::array<std::pair<uint32_t, uint32_t>, 4>{{
                {0, 0x200000}, {0x220000, 0x20000}, {0x2000000, 0x2000000}, {0x6000000, 0x1000000}}}) {
            for (uint32_t offset = 0; offset < bytes; offset += 4093) {
                const uint32_t address = first + offset;
                require(dense.board().read_byte(address) == paged.board().read_byte(address), "ROM byte/mirror", 0);
                require(dense.board().read_word(address) == paged.board().read_word(address), "ROM word/mirror", 0);
                require(dense.board().read_dword(address) == paged.board().read_dword(address), "ROM dword/mirror", 0);
                require(dense.board().flags(address) == paged.board().flags(address), "burst flag", 0);
            }
        }
        for (uint32_t address : {0x40000000u, 0x80000000u, 0xffffffffu}) {
            require(paged.board().read_dword(address) == 0 && paged.board().flags(address) == 0, "unmapped address", 0);
            paged.board().write_dword(address, 0xffffffff);
            require(paged.board().read_dword(address) == 0, "unmapped write", 0);
        }
        size_t max_polygons = 0;
        unsigned screens_checked = 0, raster_frames = 0, nonblack_frames = 0;
        uint64_t framebuffer_digest = 0xcbf29ce484222325ULL, last_framebuffer_hash = 0;
        for (unsigned frame = 0; frame < count; ++frame) {
            const bool render = frame >= render_start && frame - render_start < render_frames;
            if (dense.board().video().external_3d() == render) {
                dense.board().video().set_external_3d(!render);
                paged.board().video().set_external_3d(!render);
            }
            const auto input = input_for(frame, mode == "attract");
            dense.run_frame(input);
            paged.run_frame(input);
            require(dense.instructions() == paged.instructions() && dense.frames() == paged.frames() &&
                    dense.interrupts() == paged.interrupts(), "game execution", frame);
            require(dense.board().tgp().tgp_instructions() == paged.board().tgp().tgp_instructions(), "TGP execution", frame);
            require(!std::memcmp(dense.board().tgp().buffer(), paged.board().tgp().buffer(), 0x20000), "buffer RAM", frame);
            require(dense.board().take_sound_bytes() == paged.board().take_sound_bytes(), "sound command", frame);
            compare_video(dense.board().video(), paged.board().video(), frame);
            if (render) {
                require(dense.screen().size() == size_t(width) * height &&
                        paged.screen().size() == size_t(width) * height, "CPU framebuffer size", frame);
                require(dense.screen() == paged.screen(), "CPU framebuffer pixels", frame);
                const uint64_t hash = paged.board().video().screen_hash();
                require(dense.board().video().screen_hash() == hash, "CPU framebuffer hash", frame);
                framebuffer_digest = (framebuffer_digest ^ hash) * 0x100000001b3ULL;
                last_framebuffer_hash = hash;
                ++screens_checked;
                raster_frames += paged.board().video().rendered_now();
                nonblack_frames += std::any_of(paged.screen().begin(), paged.screen().end(),
                    [](uint32_t color) { return (color & 0xffffff) != 0; });
            }
            max_polygons = std::max(max_polygons, paged.board().video().gpu_polys().size());
        }
        require(screens_checked == render_frames, "render window coverage", count);
        if (render_frames) {
            require(raster_frames && nonblack_frames, "render window has no visible 3D frames", count);
            std::printf("CPU framebuffer parity: output=%ux%u start=%u frames=%u raster_frames=%u nonblack_frames=%u digest=%016llx last_hash=%016llx\n",
                width, height, render_start, screens_checked, raster_frames, nonblack_frames,
                (unsigned long long)framebuffer_digest, (unsigned long long)last_framebuffer_hash);
        }
        size_t cache_bytes = 0;
        const char *names[] = {"program", "main", "copro", "polygons", "textures"};
        for (size_t i = 0; i < files.size(); ++i) {
            const auto &f = *files[i];
            cache_bytes += f.cache_bytes();
            std::printf("  %s cache=%zu hits=%llu misses=%llu read_bytes=%llu\n", names[i], f.cache_bytes(),
                (unsigned long long)f.stats().hits, (unsigned long long)f.stats().misses,
                (unsigned long long)f.stats().bytes_read);
        }
        std::printf("PSP paging parity: %u frames, %llu i960 instructions, %llu TGP instructions, max polygons=%zu, cache=%zu bytes\n",
            count, (unsigned long long)paged.instructions(), (unsigned long long)paged.board().tgp().tgp_instructions(),
            max_polygons, cache_bytes);
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "PSP paging parity failed: %s\n", e.what());
        return 1;
    }
}
