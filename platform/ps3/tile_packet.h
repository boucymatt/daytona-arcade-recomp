#pragma once
#include <stdint.h>
typedef struct Ps3TilePacket {
    const uint16_t *pixels[4];
    const uint8_t *flags[4];
    uint64_t rows[4];
    const uint16_t *words;
    const uint32_t *pens;
    const uint32_t *material_colours;
    unsigned material_count;
} Ps3TilePacket;
