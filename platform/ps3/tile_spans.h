#pragma once
#include "tile_packet.h"
enum { PS3_TILE_RECTS = 32768, PS3_TILE_VERTICES = PS3_TILE_RECTS*6 };
typedef struct Ps3TileRect { int x,y,w,h,sx,sy,layer,category,flags; } Ps3TileRect;
static Ps3TileRect tile_rects[PS3_TILE_RECTS];
static unsigned tile_rect_count, tile_front_first;
static int tile_add_span(int x,int y,int w,int sx,int sy,int layer,int category,int flags,
                         const unsigned *previous,unsigned previous_count) {
    for (unsigned j=0;j<previous_count;++j) {
        unsigned i=previous[j];
        Ps3TileRect *r=&tile_rects[i];
        if(r->x==x && r->w==w && r->sx==sx && r->y+r->h==y && r->sy+r->h==sy
           && r->layer==layer && r->category==category && r->flags==flags) { ++r->h; return (int)i+1; }
    }
    if(tile_rect_count==PS3_TILE_RECTS) return 0;
    tile_rects[tile_rect_count++]=(Ps3TileRect){x,y,w,1,sx,sy,layer,category,flags};
    return (int)tile_rect_count;
}
static unsigned tile_word(const Ps3TilePacket *t,unsigned i) { return t->words[i-0x4000]; }
static int prepare_tile_spans(const Ps3TilePacket *t) {
    tile_rect_count=0;
    tile_rects[tile_rect_count++]=(Ps3TileRect){0,0,496,384,0,0,0,0,4};
    for(int category=0;category<2;++category) {
        if(category) tile_front_first=tile_rect_count;
        for(int l=3;l>=0;--l) {
            unsigned h=tile_word(t,0x5000+l),v=tile_word(t,0x5004+l);
            unsigned mode=(tile_word(t,0x5004+(l&2))>>13)&3;
            if((v&0x8000) || (mode && (l&1))) continue;
            int opaque=!category && l>=2;
            unsigned previous[128],current[128],previous_count=0;
            for(int y=0;y<384;++y) {
                unsigned current_count=0;
                unsigned hl=(h&0x8000)?tile_word(t,0x4000+l*512+y):h;
                int sy=(y+(v&511))&511;
                for(int x=0;x<496;) {
                    int src=l,n=496-x,visible=1;
                    if(mode==1) {
                        src=( (-(int)v)&512 )?l:(l^1);
                        if(y>=((-(int)v)&511)) src^=1;
                    } else if(mode) {
                        src=(hl&512)?l:(l^1);
                        int split=hl&511;
                        if(x>=split) src^=1;
                        else if(n>split-x) n=split-x;
                    } else {
                        n=8-(x&7); if(n>496-x)n=496-x;
                        unsigned mask=tile_word(t,(l>=2?0x6800:0x6000)+y*4+(x>>7));
                        visible=((mask>>(15-((x>>3)&15)))&1)==(unsigned)(l&1);
                        while(x+n<496) {
                            int xx=x+n;
                            unsigned m=tile_word(t,(l>=2?0x6800:0x6000)+y*4+(xx>>7));
                            int next=((m>>(15-((xx>>3)&15)))&1)==(unsigned)(l&1);
                            if(next!=visible)break;
                            n+=8;
                        }
                    }
                    int sx=(x-(hl&511))&511;
                    if(n>512-sx)n=512-sx;
                    if(visible) {
                        int index=tile_add_span(x,y,n,sx,sy,src,category,
                            opaque?(mode?1:3):0,previous,previous_count);
                        if(!index || current_count==128)return 0;
                        current[current_count++]=(unsigned)index-1;
                    }
                    x+=n;
                }
                for(unsigned j=0;j<current_count;++j)previous[j]=current[j];
                previous_count=current_count;
            }
        }
    }
    return 1;
}
