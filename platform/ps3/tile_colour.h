#pragma once
#include <stdint.h>
static inline uint32_t ps3_tile_colour(unsigned pen,unsigned category,unsigned variant,uint32_t colour) {
    int visible=variant==3 || (variant==2 ? category==0 : category==variant && (pen&15)!=0);
    return visible ? colour|0xff000000u : 0;
}
