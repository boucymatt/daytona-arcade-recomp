// ROM-free differential native-panel System24/HUD tests. Compile this source
// once with PSP_VIDEO_REFERENCE_ONLY and rt=psp_video_reference, together with
// reference video.cpp/raster.cpp without M2_PSP_NATIVE_VIDEO. Link that object
// into the M2_PSP_NATIVE_VIDEO build of this test; no reference pixels or game
// data are stored in the repository.
#include "runtime/video.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

extern "C" {
void *psp_video_reference_create(const uint8_t *tiles, const uint8_t *chars);
void psp_video_reference_destroy(void *video);
void psp_video_reference_draw(void *video, const uint8_t *palette, const uint8_t *translation, uint32_t *pixels);
}

#ifdef PSP_VIDEO_REFERENCE_ONLY
extern "C" void *psp_video_reference_create(const uint8_t *tiles, const uint8_t *chars) {
    return new rt::Video(tiles, chars);
}
extern "C" void psp_video_reference_destroy(void *video) {
    delete static_cast<rt::Video *>(video);
}
extern "C" void psp_video_reference_draw(void *video, const uint8_t *palette, const uint8_t *translation,
                                         uint32_t *pixels) {
    auto &v = *static_cast<rt::Video *>(video);
    for (unsigned i = 0; i < 8192; ++i) v.palette_w(i, palette, translation);
    v.frame_start();
    v.screen_update({}, 0, {palette, translation, nullptr, nullptr, nullptr});
    std::copy(v.screen().begin(), v.screen().end(), pixels);
}
#else
#ifndef M2_PSP_NATIVE_VIDEO
#error This test must exercise the native PSP output branch.
#endif
namespace {
void require(bool value, const char *what) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
uint32_t random_state = 0x95a14b73u;
uint32_t random32() {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
struct Memory {
    std::vector<uint8_t> tiles = std::vector<uint8_t>(0x10000);
    std::vector<uint8_t> chars = std::vector<uint8_t>(0x80000);
    std::vector<uint8_t> palette = std::vector<uint8_t>(0x4000);
    std::vector<uint8_t> translation = std::vector<uint8_t>(0xc000);
    void word(unsigned index, uint16_t value) {
        tiles[index * 2] = uint8_t(value); tiles[index * 2 + 1] = uint8_t(value >> 8);
    }
    Memory() {
        for (auto *memory : {&chars, &palette, &translation})
            for (auto &value : *memory) value = uint8_t(random32());
        for (unsigned i = 0; i < 0x4000; ++i) word(i, uint16_t(random32()));
    }
    void scene(unsigned frame) {
        // All 16 pairs of split modes, independently enabled line-scroll on
        // every layer, both 9-bit wrap sides and per-layer disable controls.
        constexpr std::array<unsigned, 14> edges{0, 1, 7, 8, 127, 128, 129, 255, 383, 384, 495, 496, 510, 511};
        for (unsigned layer = 0; layer < 4; ++layer) {
            const auto edge = edges[(frame + layer * 3) % edges.size()];
            word(0x5000 + layer, uint16_t(edge | ((frame & (1u << (layer + 2))) ? 0x8000 : 0)));
            word(0x5004 + layer, uint16_t(edges[(frame * 3 + layer) % edges.size()] |
                ((frame & 16) ? 0x200 : 0) | ((frame + layer) % 29 == 0 ? 0x8000 : 0)));
            for (unsigned y = 0; y < 512; ++y)
                word(0x4000 + 0x200 * layer + y, uint16_t(random32() & 0x3ff));
        }
        const auto mode0 = uint16_t((frame & 3) << 13), mode1 = uint16_t(((frame >> 2) & 3) << 13);
        auto add_mode = [&](unsigned index, uint16_t mode) {
            word(index, uint16_t(tiles[index * 2] | tiles[index * 2 + 1] << 8) | mode);
        };
        add_mode(0x5004, mode0); add_mode(0x5006, mode1);
        for (unsigned i = 0x6000; i < 0x7000; ++i)
            word(i, frame % 5 == 0 ? 0 : frame % 5 == 1 ? 0xffff : uint16_t(random32()));
        for (unsigned i = 0; i < 31; ++i) word(random32() & 0x3fff, uint16_t(random32()));
        for (unsigned i = 0; i < 7; ++i) chars[random32() % chars.size()] ^= uint8_t(random32());
        palette[random32() % palette.size()] ^= uint8_t(random32());
        translation[random32() % translation.size()] ^= uint8_t(random32());
    }
};
}

int main() {
    static_assert(rt::Video::W == 496 && rt::Video::H == 384);
    static_assert(rt::Video::OutputW == 480 && rt::Video::OutputH == 272);
    Memory memory;
    auto video = std::make_unique<rt::Video>(memory.tiles.data(), memory.chars.data());
    void *reference = psp_video_reference_create(memory.tiles.data(), memory.chars.data());
    require(video->output_width() == 480 && video->output_height() == 272, "native output dimensions");
    require(video->screen().size() == 480 * 272, "no logical-resolution screen allocation");
    require(video->raster().stride() == 480, "native raster stride");
    std::vector<uint32_t> expected(496 * 384);
    uint64_t compared = 0, colored = 0;
    for (unsigned frame = 0; frame < 128; ++frame) {
        memory.scene(frame);
        psp_video_reference_draw(reference, memory.palette.data(), memory.translation.data(), expected.data());
        for (unsigned i = 0; i < 8192; ++i) video->palette_w(i, memory.palette.data(), memory.translation.data());
        for (bool stretch : {false, true}) {
            video->set_psp_stretch(stretch);
            video->frame_start();
            video->screen_update({}, 0, {memory.palette.data(), memory.translation.data(), nullptr, nullptr, nullptr});
            const int left = stretch ? 0 : 58, width = stretch ? 480 : 363;
            for (int y = 0; y < 272; ++y)
                for (int x = 0; x < 480; ++x) {
                    uint32_t wanted = 0xff000000u;
                    if (x >= left && x < left + width) {
                        const int sx = (2 * (x - left) + 1) * 496 / (2 * width);
                        const int sy = (2 * y + 1) * 384 / (2 * 272);
                        wanted = expected[size_t(sy) * 496 + sx];
                    }
                    const uint32_t got = video->screen()[size_t(y) * 480 + x];
                    if (got != wanted) {
                        std::fprintf(stderr, "FAIL: frame=%u stretch=%d output=(%d,%d) actual=%08x reference=%08x\n",
                                     frame, int(stretch), x, y, got, wanted);
                        return 1;
                    }
                    ++compared; colored += (got & 0xffffffu) != 0;
                }
        }
    }
    require(colored > compared / 4, "nonempty palette/tile coverage");
    require(video->raster_hash() == video->raster().hash(0, 479, 0, 271), "native raster hash bounds");
    video->set_external_3d(true);
    require(video->background_layer().size() == 480 * 272 &&
            video->foreground_layer().size() == 480 * 272, "native optional GPU layer sizes");
    video->set_psp_stretch(false);
    video->screen_update({}, 0, {memory.palette.data(), memory.translation.data(), nullptr, nullptr, nullptr});
    for (int y = 0; y < 272; ++y)
        for (int x = 0; x < 480; ++x)
            if (x < 58 || x >= 421)
                require(video->screen()[size_t(y) * 480 + x] == 0xff000000u, "external mode letterbox bars");
    psp_video_reference_destroy(reference);
    std::printf("PSP native Video: %llu pixels exact against reference center sampling; 128 scenes, both aspects, normal/split/window/line-scroll/category modes\n",
                static_cast<unsigned long long>(compared));
}
#endif
