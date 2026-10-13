#include "scale.h"
#include <vector>
#include <cstdio>

int main() {
    Ps3Scaler scaler{};
    const unsigned modes[][2] = {{1920,1080},{1280,720},{720,480},{720,576},{640,480},{480,640},{1,1},{1919,1079}};
    unsigned long long checks = 0;
    for (const auto &mode : modes) {
        unsigned dw = mode[0], dh = mode[1];
        for (unsigned sw : {496u, 640u}) {
            unsigned sh = 384;
            std::vector<uint32_t> in(sw * sh), out(dw * dh + 2, 0x12345678);
            for (unsigned frame = 0; frame < 2; ++frame) {
                for (unsigned i = 0; i < in.size(); ++i) in[i] = (i * 2654435761u) ^ (frame * 0xffffffffu);
                unsigned w = dh * 4 / 3, h = dh;
                if (w > dw) { w = dw; h = dw * 3 / 4; }
                if (!w || !h) {
                    if (ps3_scale(&scaler, out.data()+1, dw, dh, in.data(), sw, sh)) return 1;
                    continue;
                }
                if (!ps3_scale(&scaler, out.data()+1, dw, dh, in.data(), sw, sh)) return 2;
                unsigned left = (dw-w)/2, top = (dh-h)/2;
                for (unsigned y = 0; y < dh; ++y)
                    for (unsigned x = 0; x < dw; ++x) {
                        uint32_t expected = x >= left && x < left+w && y >= top && y < top+h
                            ? in[((y-top)*sh/h)*sw+(x-left)*sw/w] & 0xffffff : 0;
                        if (out[1+y*dw+x] != expected) return 3;
                        ++checks;
                    }
                if (out.front() != 0x12345678 || out.back() != 0x12345678) return 4;
            }
        }
    }
    std::printf("PASS: %llu output pixels equal original scaler, including cached frames and mode changes\n", checks);
}
