#include "../platform/psp/controls.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
int main() {
    psp::Controls c;
    auto in = c.sample({});
    CHECK(in.steer == 128 && in.accel == 32 && in.brake == 32 && in.in1 == 0xaf);
    unsigned previous = 0;
    for (unsigned i = 0; i < 256; ++i) {
        in = c.sample({0, uint8_t(i)});
        CHECK(in.steer >= previous && in.steer >= 32 && in.steer <= 224); previous = in.steer;
    }
    CHECK(c.sample({0, 0}).steer == 32 && c.sample({0, 255}).steer == 224);
    CHECK(c.sample({psp::Left}).steer == 32 && c.sample({psp::Right}).steer == 224);
    CHECK(c.sample({psp::Left | psp::Right}).steer == 128);
    CHECK(c.sample({psp::Cross | psp::Square}).accel == 224);
    CHECK(c.sample({psp::Cross | psp::Square}).brake == 224);
    c.sample({}); c.sample({psp::R}); CHECK(c.gear() == 2);
    for (unsigned i = 0; i < 20; ++i) c.sample({psp::R});
    CHECK(c.gear() == 2);
    for (unsigned i = 0; i < 20; ++i) { c.sample({}); c.sample({psp::R}); }
    CHECK(c.gear() == 4);
    for (unsigned i = 0; i < 20; ++i) { c.sample({}); c.sample({psp::L}); }
    CHECK(c.gear() == 1);
    constexpr uint32_t buttons[] = {psp::Select, psp::Start, psp::Up, psp::Right, psp::Down, psp::Left,
                                   psp::L, psp::R, psp::Triangle, psp::Circle, psp::Cross, psp::Square};
    for (unsigned mask = 0; mask < 4096; ++mask) {
        uint32_t b = 0; for (unsigned i = 0; i < 12; ++i) if (mask & (1u << i)) b |= buttons[i];
        in = c.sample({b});
        CHECK(in.steer >= 32 && in.steer <= 224 && c.gear() >= 1 && c.gear() <= 4);
        if (psp::menu_chord(b)) CHECK(in.in0 == 0xff && in.accel == 32 && in.brake == 32);
        else {
            CHECK(bool(in.in0 & 1) == !bool(b & psp::Select));
            CHECK(bool(in.in0 & 0x10) == !bool(b & psp::Start));
        }
    }
    CHECK(psp::argb_to_565(0xffff0000) == 31);
    CHECK(psp::argb_to_565(0xff00ff00) == 0x7e0);
    CHECK(psp::argb_to_565(0xff0000ff) == 0xf800);
    CHECK(psp::argb_to_565(0xffffffff) == 0xffff && psp::argb_to_565(0xff000000) == 0);
    for (unsigned i = 0; i < 256; ++i) CHECK(psp::argb_to_565(0xff000000u | i * 0x10101u) ==
        uint16_t((i >> 3) | ((i >> 2) << 5) | ((i >> 3) << 11)));
    CHECK(psp::viewport(false).x == 58 && psp::viewport(false).width == 363);
    CHECK(psp::viewport(true).x == 0 && psp::viewport(true).width == 480);
    std::puts("PSP controls/presentation: 256 axes,4096 button combinations,RGB565 and viewport passed");
}
