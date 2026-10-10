/* Native XAM packet decoder. Public ABI reference recorded in THIRD_PARTY.md. */
#ifndef DAYTONA_XBOX360_CONTROLLER_H
#define DAYTONA_XBOX360_CONTROLLER_H
#include <stdint.h>

typedef struct {
    uint32_t packet;
    uint16_t buttons;
    uint8_t left_trigger, right_trigger;
    int32_t left_x;
} DaytonaPad;

static inline uint16_t pad_be16(const unsigned char *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static inline DaytonaPad decode_pad(const unsigned char raw[16]) {
    DaytonaPad pad;
    pad.packet = ((uint32_t)raw[0] << 24) | ((uint32_t)raw[1] << 16) |
                 ((uint32_t)raw[2] << 8) | raw[3];
    pad.buttons = pad_be16(raw + 4);
    pad.left_trigger = raw[6];
    pad.right_trigger = raw[7];
    uint32_t x = pad_be16(raw + 8);
    pad.left_x = x >= 32768 ? (int32_t)x - 65536 : (int32_t)x;
    return pad;
}

/* ADC conversion only; game integration and button bindings are a later gate. */
static inline uint8_t pad_steer(const DaytonaPad *pad) {
    const int32_t deadzone = 7849;
    int32_t x = pad->left_x;
    if ((pad->buttons & 12) == 4) return 0x20;
    if ((pad->buttons & 12) == 8) return 0xe0;
    if (x > deadzone) return (uint8_t)(128 + (x - deadzone) * 96 / (32767 - deadzone));
    if (x < -deadzone) return (uint8_t)(128 - (-x - deadzone) * 96 / (32768 - deadzone));
    return 0x80;
}
static inline uint8_t pad_pedal(uint8_t value) {
    return (uint8_t)(0x20 + ((uint32_t)value * 0xc0 + 127) / 255);
}
#endif
