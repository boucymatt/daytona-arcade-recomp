#pragma once
#include <stdint.h>
#include <string.h>

/* CPU nearest-neighbour reference output, with no framebuffer readbacks. */
typedef struct {
    unsigned dw, dh, sw, sh, width, height, left, top;
    unsigned xmap[1920];
    uint32_t row[1920];
} Ps3Scaler;

static int ps3_scale(Ps3Scaler *s, uint32_t *out, unsigned dw, unsigned dh,
                     const uint32_t *in, unsigned sw, unsigned sh) {
    if (!dw || dw > 1920 || !dh || !sw || !sh || !out || !in) return 0;
    if (s->dw != dw || s->dh != dh || s->sw != sw || s->sh != sh) {
        s->dw = dw; s->dh = dh; s->sw = sw; s->sh = sh;
        s->width = dh * 4 / 3; s->height = dh;
        if (s->width > dw) { s->width = dw; s->height = dw * 3 / 4; }
        if (!s->width || !s->height) return 0;
        s->left = (dw - s->width) / 2; s->top = (dh - s->height) / 2;
        for (unsigned x = 0; x < s->width; ++x) s->xmap[x] = x * sw / s->width;
    }
    if (!s->width || !s->height) return 0;
    memset(s->row, 0, dw * sizeof(uint32_t));
    unsigned previous = sh;
    for (unsigned y = 0; y < dh; ++y) {
        if (y >= s->top && y < s->top + s->height) {
            unsigned sy = (y - s->top) * sh / s->height;
            if (sy != previous) {
                const uint32_t *src = in + sy * sw;
                for (unsigned x = 0; x < s->width; ++x)
                    s->row[s->left + x] = src[s->xmap[x]] & 0xffffff;
                previous = sy;
            }
        } else if (previous != sh) {
            memset(s->row, 0, dw * sizeof(uint32_t));
            previous = sh;
        }
        memcpy(out + y * dw, s->row, dw * sizeof(uint32_t));
    }
    return 1;
}
