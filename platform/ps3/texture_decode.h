#pragma once
#include <stdint.h>
/* Physical sheet layout: 1024 by 4096 nibbles in guest-LE words. */
static inline uint8_t ps3_texture_index(const uint8_t *sheet, unsigned x, unsigned y) {
    unsigned offset = (y / 2) * 512 + x / 2;
    unsigned byte = ((offset >> 1) & 0x7ffff) * 4
                  + (offset & 1) * 2 + ((y & 1) ? 0 : 1);
    return (sheet[byte] >> ((x & 1) ? 0 : 4)) & 15;
}
