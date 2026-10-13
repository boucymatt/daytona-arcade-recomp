#pragma once
#include <stdint.h>
/* Buffers contain aligned uint32_t pixels, not arbitrary byte strings. */
static inline int ps3_layer_row_equal(const uint32_t *a, const uint32_t *b) {
    for (unsigned x=0; x<496; x+=4)
        if ((a[x]^b[x]) | (a[x+1]^b[x+1]) | (a[x+2]^b[x+2]) | (a[x+3]^b[x+3])) return 0;
    return 1;
}
