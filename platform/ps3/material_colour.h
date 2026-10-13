#pragma once
#include <stdint.h>
static inline unsigned ps3_gamma_byte(unsigned value) {
    return value>64 ? ((value-64)*255)/191 : 0;
}
static inline uint32_t ps3_material_colour(const uint8_t *lumaram, const uint8_t *xlat,
                                           unsigned colour, unsigned base, unsigned polygon_luma,
                                           unsigned nibble) {
    unsigned light=(uint8_t)((unsigned)lumaram[(base+nibble*8)*4]*polygon_luma/256);
    if(light>63) light=63;
    uint32_t result=0xff000000u;
    for(unsigned c=0;c<3;++c) {
        unsigned offset=(c*0x2000+(((colour>>(c*5))&31)<<8)+light)*2;
        unsigned value=xlat[offset]; // colour translation uses the low byte
        unsigned channel=ps3_gamma_byte(value);
        result |= channel << (16-c*8);
    }
    return result;
}
