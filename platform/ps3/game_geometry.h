#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>
#include <cstring>
#include "geometry_batch.h"
#include "tile_packet.h"
#include "material_cache.h"
extern "C" int ps3_present_geometry(const float *, unsigned, const Ps3GeometryBatch *, unsigned,
                                    const void *, const void *, uint64_t, const Ps3TilePacket *);
static bool ps3_draw_game_geometry(const rt::Video &video) {
    const auto &polys = video.gpu_polys();
    const auto &mem = video.gpu_mem();
    static uint64_t tile_generation = 0;
    static bool tiles_valid = false;
    Ps3TilePacket tiles = {};
    tiles.words = video.gpu_tile_words(); tiles.pens = video.gpu_pens();
    for (int l=0; l<4; ++l) {
        tiles.pixels[l] = video.system24_pixels(l); tiles.flags[l] = video.system24_flags(l);
        for (unsigned t=0; t<4096; ++t)
            if (!tiles_valid || video.system24_tile_generation(l,t) > tile_generation)
                tiles.rows[l] |= uint64_t(1) << (t/64);
    }
    tile_generation = video.system24_texture_generation(); tiles_valid = true;
    if (!mem.palram || !mem.colorxlat || !mem.lumaram || polys.size()>65536) return false;
    static Ps3MaterialCache materials;
    materials.begin();
    auto le16 = [](const uint8_t *p, unsigned i) { return unsigned(p[i*2]) | unsigned(p[i*2+1]) << 8; };
    auto gamma = [](unsigned x) { return float(ps3_gamma_byte(x)) / 255.f; };
    static std::vector<size_t> order;
    order.resize(polys.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (polys[a].window != polys[b].window) return polys[a].window > polys[b].window;
        if (polys[a].z != polys[b].z) return polys[a].z < polys[b].z;
        return a > b;
    });
    static std::vector<float> vertices;
    static std::vector<Ps3GeometryBatch> batches;
    size_t used = 0;
    batches.clear();
    unsigned rank = 0;
    for (size_t i : order) {
        const auto &p = polys[i];
        ++rank;
        if (p.window > video.gpu_windows() || p.num_vertices < 3 || p.num_vertices > 8) continue;
        const unsigned renderer = (p.texheader[0] >> 13) & 3;
        if (renderer == 1) continue; // Reference solid-translucent path draws nothing.
        const int x0 = std::max(p.viewport[0] + video.render_x(), 0);
        const int x1 = std::min(p.viewport[2] + video.render_x(), 495) + 1;
        const int y0 = std::max(384 - p.viewport[3] + video.render_y(), 0);
        const int y1 = std::min(384 - p.viewport[1] + video.render_y(), 383) + 1;
        if (x0 >= x1 || y0 >= y1) continue;
        const unsigned colour = le16(mem.palram, ((p.texheader[3] >> 6) & 0x3ff) + 0x1000);
        const unsigned luma = p.luma >> 2;
        float rgb[3];
        for (unsigned c=0; c<3; ++c)
            rgb[c] = gamma(le16(mem.colorxlat, c*0x2000 + (((colour >> (c*5)) & 31) << 8) + luma) & 255);
        const unsigned material=renderer>=2 ? materials.get(mem.lumaram,mem.colorxlat,
            colour,p.texheader[1]&255,p.luma) : 0;
        const float depth=2.f*float(rank)/float(order.size()+2)-1.f;
        const float center_x=float(video.crtc_x()+p.center[0]);
        const float center_y=float(384-p.center[1]+video.crtc_y());
        float projected[8][20]; bool valid = true;
        for (int k = 0; k < p.num_vertices; ++k) {
            const auto &v = p.v[k];
            float z = v.p[0] + std::numeric_limits<float>::min();
            float x = center_x + v.x / z;
            float y = center_y - v.y / z;
            valid &= std::isfinite(x) && std::isfinite(y);
            projected[k][0] = x / 248.f - 1.f;
            projected[k][1] = 1.f - y / 192.f;
            projected[k][2] = depth;
            projected[k][3] = 1;
            // Real solid colour; flat material approximation for textured polys.
            projected[k][4] = rgb[0]; projected[k][5] = rgb[1];
            projected[k][6] = rgb[2]; projected[k][7] = float((p.texheader[0] >> 15) & 1);
            // Interpolate u/z, v/z and 1/z, then divide in the fragment shader.
            // Clip W stays 1 so the synthetic ordering depth is unchanged.
            projected[k][8] = v.p[1] / z / 8.f;
            projected[k][9] = v.p[2] / z / 8.f;
            projected[k][10] = 1.f / z;
            projected[k][11] = float(renderer);
            projected[k][12] = float(32 * (p.texheader[2] & 63));
            projected[k][13] = float(32 * ((p.texheader[2] >> 6) & 31));
            projected[k][14] = float(32 << (p.texheader[0] & 7));
            projected[k][15] = float(32 << ((p.texheader[0] >> 3) & 7));
            projected[k][16] = float((p.texheader[0] >> 8) & 1);
            projected[k][17] = float((p.texheader[0] >> 9) & 1);
            projected[k][18] = float((p.texheader[2] >> 12) & 1);
            projected[k][19] = float(material);
            valid &= std::isfinite(projected[k][8]) && std::isfinite(projected[k][9])
                && std::isfinite(projected[k][10]);
        }
        if (!valid) continue;
        const size_t added=size_t(p.num_vertices-2)*3*20;
        if (used/20 + added/20 > 131072) return false;
        // Retain staging storage across frames. Avoid vector range insertion
        // and per-vertex capacity bookkeeping for every triangle.
        if(vertices.size()<used+added) vertices.resize(used+added);
        if (batches.empty() || batches.back().x0 != x0 || batches.back().x1 != x1
            || batches.back().y0 != y0 || batches.back().y1 != y1)
            batches.push_back({unsigned(used/20), 0, x0, y0, x1, y1});
        batches.back().count += unsigned(p.num_vertices - 2) * 3;
        for (int k = 1; k + 1 < p.num_vertices; ++k) {
            std::memcpy(vertices.data()+used,projected[0],20*sizeof(float));
            std::memcpy(vertices.data()+used+20,projected[k],20*sizeof(float));
            std::memcpy(vertices.data()+used+40,projected[k+1],20*sizeof(float));
            used+=60;
        }
    }
    tiles.material_colours=materials.colours.data();
    tiles.material_count=unsigned(materials.colours.size()/16);
    return ps3_present_geometry(vertices.data(), unsigned(used / 20), batches.data(), unsigned(batches.size()),
                                mem.tex0, mem.tex1, mem.tex_generation,
                                &tiles);
}
