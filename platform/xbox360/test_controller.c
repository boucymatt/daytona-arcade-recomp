#include "controller.h"
#include <stdio.h>
int main(void) {
    unsigned char raw[16] = {0x12, 0x34, 0x56, 0x78, 0x10, 0, 0, 255, 0x80, 0};
    DaytonaPad p = decode_pad(raw);
    if (p.packet != 0x12345678 || p.buttons != 0x1000 || p.left_x != -32768 ||
        pad_steer(&p) != 0x20 || pad_pedal(p.right_trigger) != 0xe0 || pad_pedal(0) != 0x20) return 1;
    unsigned prev = 0;
    for (int32_t x = -32768; x <= 32767; ++x) {
        p.left_x = x;
        unsigned out = pad_steer(&p);
        if (out < 0x20 || out > 0xe0 || out < prev) return 2;
        if (x >= -7849 && x <= 7849 && out != 0x80) return 3;
        prev = out;
    }
    if (prev != 0xe0) return 4;
    p.buttons = 4; if (pad_steer(&p) != 0x20) return 5;
    p.buttons = 8; if (pad_steer(&p) != 0xe0) return 6;
    puts("Controller packet, trigger and 65536 steering cases passed (host only)");
    return 0;
}
