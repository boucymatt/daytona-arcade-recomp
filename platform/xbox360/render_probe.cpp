// Synthetic pixels through the shipping CPU rasterizer, no game/ROM data.
#include "runtime/raster.h"
#include "runtime/raster_texel.h"
#include "tls_runtime.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#ifndef XBOX_RENDER_HOST_TEST
#include "render_expected.h"
int run_exception_cases();
#endif

static constexpr unsigned kCases = 16;
static void put16(std::vector<uint8_t> &data, size_t at, uint16_t v) {
    data[at] = uint8_t(v); data[at + 1] = uint8_t(v >> 8);
}
static uint32_t scalar(const uint8_t *sheet, uint32_t bx, uint32_t by, uint32_t u, uint32_t v) {
    uint32_t x = bx + u, y = by + v;
    if (x >= 1024) { x -= 1024; y ^= 1024; }
    uint32_t offset = (y / 2) * 512 + x / 2;
    uint32_t word = ((offset >> 1) & 0x7ffff) * 4;
    unsigned shift = ((offset & 1) * 16) + ((1 ^ (v & 1)) * 8) + ((1 ^ (u & 1)) * 4);
    // Independent byte/nibble oracle, not guest_load or native word reads.
    return ((sheet[word + shift / 8] >> (shift & 7)) & 15) << 4;
}
static bool save_ppm(const char *path, const rt::Raster &raster) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;
    bool ok = fprintf(file, "P6\n496 384\n255\n") > 0;
    uint8_t row[496 * 3];
    for (unsigned y = 0; y < 384 && ok; ++y) {
        for (unsigned x = 0; x < 496; ++x) {
            uint32_t pixel = raster.pixels()[y * raster.stride() + x];
            row[x * 3] = uint8_t(pixel >> 16);
            row[x * 3 + 1] = uint8_t(pixel >> 8);
            row[x * 3 + 2] = uint8_t(pixel);
        }
        ok = fwrite(row, 1, sizeof(row), file) == sizeof(row);
    }
    return fclose(file) == 0 && ok;
}
static bool run(FILE *log) {
    std::vector<uint32_t> sheets[2] = {std::vector<uint32_t>(0x80000), std::vector<uint32_t>(0x80000)};
    for (unsigned s = 0; s < 2; ++s) {
        auto *bytes = reinterpret_cast<uint8_t *>(sheets[s].data());
        for (unsigned i = 0; i < 0x200000; ++i) bytes[i] = uint8_t((i * 37 + (i >> 7) * 11 + s * 83) ^ (i >> 13));
    }
    uint32_t state = 0x578923;
    auto next = [&]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned i = 0; i < 20000; ++i) {
        unsigned s = i & 1;
        uint32_t bx = next() % 2048, by = next() % 1024, u = next() % 4096, v = next() % 4096;
        uint32_t u1 = (u + 1) % 4096, v1 = (v + 1) % 4096;
        auto quad = rt::read_texel_quad(bx, by, u, u1, v, v1, sheets[s].data());
        auto *bytes = reinterpret_cast<const uint8_t *>(sheets[s].data());
        if (quad.t00 != scalar(bytes,bx,by,u,v) || quad.t01 != scalar(bytes,bx,by,u1,v) ||
            quad.t10 != scalar(bytes,bx,by,u,v1) || quad.t11 != scalar(bytes,bx,by,u1,v1))
            throw std::runtime_error("texture nibble/byte-order mismatch");
    }
    if (log) { fprintf(log, "80000 byte-oracle texel comparisons PASS\n"); fflush(log); }
    std::vector<uint8_t> palette(0x4000), xlat(0xc000), luma(0x20000);
    for (unsigned i = 0; i < palette.size() / 2; ++i) put16(palette, i * 2, uint16_t(i * 137));
    for (unsigned i = 0; i < xlat.size() / 2; ++i) put16(xlat, i * 2, uint16_t(64 + ((i & 63) * 3 + ((i >> 8) & 31)) % 192));
    for (unsigned i = 0; i < luma.size(); i += 4) luma[i] = uint8_t((i / 4) & 63);
    rt::VideoMem memory;
    memory.palram = palette.data(); memory.colorxlat = xlat.data(); memory.lumaram = luma.data();
    memory.tex0 = sheets[0].data(); memory.tex1 = sheets[1].data();
    rt::Raster raster;
    bool ok = true;
#ifdef XBOX_RENDER_HOST_TEST
    printf("static constexpr unsigned long long render_expected[] = {\n");
#endif
    for (unsigned test = 0; test < kCases; ++test) {
        rt::GeoPoly poly{};
        poly.num_vertices = (test & 1) ? 3 : 4;
        poly.z = 1; poly.luma = 255;
        poly.viewport[0] = 0; poly.viewport[1] = 0; poly.viewport[2] = 495; poly.viewport[3] = 384;
        poly.texheader[0] = uint16_t(0x4000 | ((test & 2) ? 0x8000 : 0) | ((test & 4) ? 0x300 : 0));
        poly.texheader[2] = uint16_t(((test & 8) ? 0x1000 : 0) | (test * 3));
        poly.texheader[3] = uint16_t((test + 1) << 6);
        const float x[4] = {32, 464, 432, 64}, y[4] = {32, 64, 352, 320};
        for (unsigned v = 0; v < poly.num_vertices; ++v) {
            float z = test & 8 ? float(1 + (v & 1)) : 1.0f;
            poly.v[v].x = x[v] * z;
            poly.v[v].y = (384 - y[v]) * z;
            poly.v[v].p[0] = z;
            poly.v[v].p[1] = float((v == 1 || v == 2) ? 512 : 0);
            poly.v[v].p[2] = float(v >= 2 ? 512 : 0);
        }
        raster.render({poly}, 1, memory, 0, 0, 0, 495, 0, 383);
        unsigned drawn = 0;
        for (unsigned y = 0; y < 384; ++y)
            for (unsigned x = 0; x < 496; ++x) drawn += raster.pixels()[y * raster.stride() + x] != 0;
        if (drawn < 1000) throw std::runtime_error("synthetic raster case is blank");
        uint64_t hash = raster.hash(0, 495, 0, 383);
#ifdef XBOX_RENDER_HOST_TEST
        printf("0x%08x%08xull,\n", unsigned(hash >> 32), unsigned(hash));
#else
        bool match = hash == render_expected[test];
        ok &= match;
        fprintf(log, "Frame %u pixels=%u hash=%08x%08x expected=%08x%08x %s\n", test, drawn,
            unsigned(hash >> 32), unsigned(hash), unsigned(render_expected[test] >> 32),
            unsigned(render_expected[test]), match ? "PASS" : "FAIL");
        if (fflush(log) != 0) ok = false;
#endif
    }
#ifdef XBOX_RENDER_HOST_TEST
    printf("};\n");
    ok &= save_ppm("/tmp/daytona-render-reference.ppm", raster);
#else
    ok &= save_ppm("game:\\xbox360-raster.ppm", raster);
#endif
    return ok;
}
int main() {
#ifdef XBOX_RENDER_HOST_TEST
    return run(nullptr) ? 0 : 1;
#else
    FILE *log = fopen("game:\\xbox360-render.log", "wb");
    if (!log) return 1;
    fprintf(log, "Render probe START: synthetic CPU framebuffer; no GPU display or game\n");
    bool ok = fflush(log) == 0 && xbox_tls_init();
    if (ok) {
        try {
            if (run_exception_cases()) throw std::runtime_error("exception regression");
            ok = run(log);
        } catch (const std::exception &error) { fprintf(log, "FAIL: %s\n", error.what()); ok = false; }
        catch (...) { fprintf(log, "FAIL: unknown exception\n"); ok = false; }
        xbox_tls_thread_cleanup();
        ok &= xbox_tls_shutdown() != 0;
    }
    fprintf(log, "Render probe complete: %s\n", ok ? "PASS" : "FAIL");
    return fclose(log) == 0 && ok ? 0 : 1;
#endif
}
