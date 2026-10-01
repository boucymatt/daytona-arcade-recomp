// ROM-free native-resolution raster checks. Build with M2_PSP_NATIVE_VIDEO;
// synthetic guest geometry is projected directly into 480x272 output pixels.
#include "runtime/raster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#ifndef M2_PSP_NATIVE_VIDEO
#error This test requires the PSP native-resolution renderer.
#endif

namespace {
void require(bool ok, const char *what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
struct Memory {
    std::vector<uint8_t> palette = std::vector<uint8_t>(0x4000);
    std::vector<uint8_t> xlat = std::vector<uint8_t>(0xc000);
    std::vector<uint8_t> luma = std::vector<uint8_t>(0x20000);
    std::vector<uint32_t> tex0 = std::vector<uint32_t>(0x80000);
    std::vector<uint32_t> tex1 = std::vector<uint32_t>(0x80000);
    Memory() {
        palette[0x2000] = 0xff; palette[0x2001] = 0x7f;
        for (size_t i = 0; i < xlat.size() / 2; ++i)
            xlat[i * 2] = uint8_t(std::min<size_t>(255, 64 + 3 * (i & 255)));
        for (size_t i = 0; i < luma.size() / 4; ++i) luma[i * 4] = uint8_t((i & 127) / 2);
    }
    rt::VideoMem view() const { return {palette.data(), xlat.data(), luma.data(), tex0.data(), tex1.data()}; }
    void constant_texture(unsigned value) { std::fill(tex0.begin(), tex0.end(), value * 0x11111111u); }
    void gradient_texture() {
        std::fill(tex0.begin(), tex0.end(), 0);
        for (unsigned y = 0; y < 32; ++y) for (unsigned x = 0; x < 32; ++x) {
            const unsigned offset = (y / 2) * 512 + x / 2;
            const unsigned shift = (offset & 1) * 16 + ((y & 1) ? 0 : 8) + ((x & 1) ? 0 : 4);
            tex0[offset / 2] |= (x / 2) << shift;
        }
    }
};
struct Point { float x, y, z = 1, u = 0; };
rt::GeoPoly polygon(std::initializer_list<Point> points, bool texture = false, bool checker = false) {
    rt::GeoPoly p;
    p.num_vertices = uint8_t(points.size());
    p.luma = 255; p.texlod = 4096; // level zero throughout the synthetic depth range
    p.viewport[0] = 0; p.viewport[1] = 1; p.viewport[2] = 495; p.viewport[3] = 384;
    p.texheader[0] = uint16_t((texture ? 0x4000 : 0) | (checker ? 0x8000 : 0));
    unsigned i = 0;
    for (const Point &v : points) {
        p.v[i].x = v.x * v.z; p.v[i].y = (384.f - v.y) * v.z;
        p.v[i].p[0] = v.z; p.v[i].p[1] = v.u * 8; p.v[i].p[2] = 16 * 8;
        ++i;
    }
    return p;
}
rt::GeoPoly full(bool texture = false, bool checker = false) {
    return polygon({{-100, -100}, {600, -100}, {600, 500}, {-100, 500}}, texture, checker);
}
bool content(int x, bool stretch) { return stretch || (x >= 58 && x < 421); }
double guest_x(int x, bool stretch) { return (x + .5 - (stretch ? 0 : 58)) * 496 / (stretch ? 480 : 363); }
double guest_y(int y) { return (y + .5) * 384 / 272; }
uint32_t pixel(const rt::Raster &r, int x, int y) { return r.pixels()[size_t(y) * r.stride() + size_t(x)]; }
void draw(rt::Raster &r, const Memory &m, const std::vector<rt::GeoPoly> &p,
          int minx = 0, int maxx = 495, int miny = 0, int maxy = 383) {
    r.render(p, 0, m.view(), 0, 0, minx, maxx, miny, maxy);
}

void coverage_and_clipping(rt::Raster &r, Memory &m, bool stretch) {
    r.set_psp_stretch(stretch);
    draw(r, m, {full()});
    for (int y = 0; y < 272; ++y) for (int x = 0; x < 480; ++x)
        require(bool(pixel(r, x, y)) == content(x, stretch), "full quad and letterbox coverage");
    // Inclusive guest scissor limits, including an exact half-pixel boundary,
    // one-pixel windows which disappear when minified, and viewport edges.
    for (const auto &clip : std::array<std::array<int, 4>, 6>{{
            {101, 299, 50, 151}, {248, 248, 192, 192}, {1, 1, 1, 1},
            {0, 0, 0, 0}, {495, 495, 383, 383}, {37, 455, 23, 349}}}) {
        auto q = full();
        q.viewport[0] = int16_t(clip[0]); q.viewport[2] = int16_t(clip[1]);
        q.viewport[3] = int16_t(384 - clip[2]); q.viewport[1] = int16_t(384 - clip[3]);
        draw(r, m, {q});
        for (int y = 0; y < 272; ++y) for (int x = 0; x < 480; ++x) {
            const double gx = guest_x(x, stretch), gy = guest_y(y);
            const bool expected = content(x, stretch) && gx >= clip[0] && gx < clip[1] + 1 &&
                                  gy >= clip[2] && gy < clip[3] + 1;
            require(bool(pixel(r, x, y)) == expected, "guest viewport maps to native pixel centers");
        }
    }
    draw(r, m, {full()}, 150, 270, 80, 120);
    for (int y = 0; y < 272; ++y) for (int x = 0; x < 480; ++x) {
        const double gx = guest_x(x, stretch), gy = guest_y(y);
        require(bool(pixel(r, x, y)) == (content(x, stretch) && gx >= 150 && gx < 271 && gy >= 80 && gy < 121),
                "caller clip remains guest coordinates");
    }
    draw(r, m, {full()}, 400, 200, 30, 20);
    require(std::all_of(r.pixels(), r.pixels() + 480 * 272, [](uint32_t p) { return p == 0; }), "empty clip writes nothing");
    draw(r, m, {polygon({{-200, -300}, {-20, -200}, {-10, -10}})});
    require(std::all_of(r.pixels(), r.pixels() + 480 * 272, [](uint32_t p) { return p == 0; }), "offscreen triangle writes nothing");
}

void checker_and_triangle(rt::Raster &r, Memory &m, bool stretch) {
    r.set_psp_stretch(stretch);
    m.constant_texture(5);
    for (bool texture : {false, true}) {
        draw(r, m, {full(texture, true)});
        for (int y = 0; y < 272; ++y) for (int x = 0; x < 480; ++x) {
            const bool expected = content(x, stretch) && ((int(std::floor(guest_x(x, stretch))) ^
                                                          int(std::floor(guest_y(y)))) & 1);
            require(bool(pixel(r, x, y)) == expected, "checker pattern uses guest pixel phase");
        }
    }
    // Different vertex depths still project onto the same guest triangle.
    draw(r, m, {polygon({{0, 0, 1}, {496, 0, 4}, {0, 384, 2}})});
    for (int y = 0; y < 272; ++y) for (int x = 0; x < 480; ++x) {
        const double edge = guest_x(x, stretch) / 496 + guest_y(y) / 384;
        if (std::abs(edge - 1) < .01) continue; // edge setup rounds to native pixel centers
        require(bool(pixel(r, x, y)) == (content(x, stretch) && edge < 1), "triangle perspective projection and clipping");
    }
}

unsigned expected_gray(double u) {
    const int fixed = int(u * 256) - 128;
    const unsigned first = unsigned(fixed >> 8) & 31;
    const unsigned fraction = unsigned(fixed) & 255;
    const unsigned t0 = (first / 2) * 16, t1 = ((first + 1) / 2) * 16;
    const unsigned t = (t0 * (256 - fraction) + t1 * fraction) >> 8;
    const unsigned level = ((t >> 1) / 2) * 255 / 256;
    return (3 * level * 255) / 191;
}
void perspective_textures(rt::Raster &r, Memory &m, bool stretch) {
    r.set_psp_stretch(stretch); m.gradient_texture();
    for (bool triangle : {false, true}) {
        const auto q = triangle ? polygon({{0, 0, 1, 0}, {496, 0, 4, 31}, {0, 384, 1, 0}}, true) :
            polygon({{0, 0, 1, 0}, {496, 0, 4, 31}, {496, 384, 4, 31}, {0, 384, 1, 0}}, true);
        draw(r, m, {q});
        unsigned checked = 0;
        for (int y = 4; y < 268; y += 7) for (int x = 0; x < 480; ++x) {
            if (!content(x, stretch)) continue;
            const double t = guest_x(x, stretch) / 496;
            if (t < .1 || t > .9 || (triangle && t + guest_y(y) / 384 > .98)) continue;
            const double u = (31 * .25 * t) / (1 - .75 * t);
            const unsigned expected = expected_gray(u), actual = pixel(r, x, y) & 255;
            // The independent double oracle may straddle one fixed-UV/lighting
            // quantization boundary relative to accumulated native floats.
            require(std::abs(int(actual) - int(expected)) <= 5, "perspective texture sampled at native pixel center");
            require((pixel(r, x, y) >> 24) == 255, "textured primitive has no holes");
            ++checked;
        }
        require(checked > 1000, "perspective oracle covered enough pixels");
    }
}
}

int main() {
    static_assert(rt::Raster::kStride == 480 && rt::Raster::kHeight == 272);
    rt::Raster raster;
    Memory memory;
    require(raster.stride() == 480, "native framebuffer stride");
    for (bool stretch : {false, true, false}) {
        coverage_and_clipping(raster, memory, stretch);
        checker_and_triangle(raster, memory, stretch);
        perspective_textures(raster, memory, stretch);
    }
    for (unsigned invalid : {9u, 16u, 255u}) {
        auto bad = full(); bad.num_vertices = uint8_t(invalid);
        bool caught = false;
        try { draw(raster, memory, {bad}); } catch (const rt::GeoFatal&) { caught = true; }
        require(caught, "invalid vertex count rejected before indexing");
    }
    std::vector<std::string> events;
    raster.set_render_observer([](void* context, const char* name, size_t, size_t) {
        static_cast<std::vector<std::string>*>(context)->emplace_back(name);
    }, &events);
    draw(raster, memory, {full()});
    require(events == std::vector<std::string>{"raster_clear_begin", "raster_order_begin",
        "raster_sort_begin", "raster_draw_begin", "raster_batch", "raster_end"}, "raster trace order");
    raster.set_render_observer(nullptr, nullptr);
    std::puts("PSP raster: native480x272, aspect/stretch, guest clipping, checker phase, triangles/quads and perspective textures passed");
}
