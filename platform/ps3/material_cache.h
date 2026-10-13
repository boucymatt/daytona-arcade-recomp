#pragma once
#include "material_colour.h"
#include <array>
#include <vector>

// Frame-local: RAM may change between frames. Collision replacement never
// overwrites colour rows already referenced by vertices in this frame.
class Ps3MaterialCache {
    struct Entry { uint32_t key=~0u, row=0; };
    std::array<Entry,4096> entries_{};
public:
    std::vector<uint32_t> colours;
    void begin() {
        for(auto &entry:entries_) entry.key=~0u;
        colours.clear();
    }
    unsigned get(const uint8_t *luma, const uint8_t *xlat,
                 unsigned colour, unsigned bank, unsigned light) {
        const uint32_t key=(colour&32767)|((light&255)<<15)|((bank&255)<<23);
        auto &entry=entries_[(key*2654435761u)>>20];
        if(entry.key==key) return entry.row;
        const unsigned row=unsigned(colours.size()/16);
        for(unsigned n=0;n<16;++n)
            colours.push_back(ps3_material_colour(luma,xlat,colour,(bank&255)<<7,light,n));
        entry={key,row};
        return row;
    }
};
