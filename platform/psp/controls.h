// PSP input mapping; no SDK dependency so the complete mapping is host-testable.
#pragma once
#include <algorithm>
#include <cstdint>

namespace psp {
enum Button : uint32_t {
    Select = 0x000001, Start = 0x000008, Up = 0x000010, Right = 0x000020,
    Down = 0x000040, Left = 0x000080, L = 0x000100, R = 0x000200,
    Triangle = 0x001000, Circle = 0x002000, Cross = 0x004000, Square = 0x008000
};
struct Pad { uint32_t buttons = 0; uint8_t lx = 128; };
struct Input {
    uint8_t steer = 128, accel = 32, brake = 32;
    uint8_t in0 = 0xff, in1 = 0x8f, in2 = 0xff;
};
inline bool menu_chord(uint32_t buttons) {
    return (buttons & (Start | Select)) == (Start | Select);
}
class Controls {
public:
    Input sample(Pad pad) {
        Input in;
        const uint32_t pressed = pad.buttons & ~held_;
        held_ = pad.buttons;
        if (!menu_chord(pad.buttons)) {
            // Integer deadzone avoids libc floating point in the input loop.
            const int x = int(pad.lx) - 128;
            const int magnitude = x < 0 ? -x : x;
            int steering = magnitude <= 16 ? 0 : (magnitude - 16) * 96 / (x < 0 ? 112 : 111);
            if (x < 0) steering = -steering;
            if (pad.buttons & (Left | Right))
                steering = 96 * (int(bool(pad.buttons & Right)) - int(bool(pad.buttons & Left)));
            in.steer = uint8_t(128 + steering);
            if (pad.buttons & Cross) in.accel = 224;
            if (pad.buttons & Square) in.brake = 224;
            gear_ = std::clamp(gear_ + int(bool(pressed & R)) - int(bool(pressed & L)), 1, 4);
            if (pad.buttons & Select) in.in0 &= uint8_t(~0x01);
            if (pad.buttons & Start) in.in0 &= uint8_t(~0x10);
            if (pad.buttons & Circle) in.in0 &= uint8_t(~0x20);
            if (pad.buttons & Triangle) in.in0 &= uint8_t(~0x40);
            if (pad.buttons & Up) in.in0 &= uint8_t(~0x80);
            if (pad.buttons & Down) in.in1 &= uint8_t(~0x01);
        }
        constexpr uint8_t gears[] = {0, 2, 1, 6, 5};
        in.in1 = uint8_t((in.in1 & ~0x70) | (gears[gear_] << 4));
        return in;
    }
    void latch(uint32_t buttons) { held_ = buttons; }
    int gear() const { return gear_; }
private:
    uint32_t held_ = 0;
    int gear_ = 1;
};

// PSP GU's 5650 format stores red in the low bits, unlike a 0xAARRGGBB word.
constexpr uint16_t argb_to_565(uint32_t color) {
    return uint16_t(((color >> 19) & 31) | ((color >> 5) & 0x07e0) | ((color << 8) & 0xf800));
}
struct Viewport { int x, y, width, height; };
constexpr Viewport viewport(bool stretch) {
    return stretch ? Viewport{0, 0, 480, 272} : Viewport{58, 0, 363, 272};
}
} // namespace psp
