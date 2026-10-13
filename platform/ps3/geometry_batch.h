#pragma once
#include <stdint.h>
/* Source-space clip edges are half-open, in 496x384 coordinates. */
typedef struct Ps3GeometryBatch {
    uint32_t first, count;
    int x0, y0, x1, y1;
} Ps3GeometryBatch;
