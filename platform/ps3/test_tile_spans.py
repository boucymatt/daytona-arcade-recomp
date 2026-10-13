import pathlib
import subprocess
import tempfile
import unittest


class TileSpans(unittest.TestCase):
    def test_coverage_wrap_and_bounds(self):
        source = r'''
#include "tile_spans.h"
#include <string.h>
static uint16_t words[12288];
static unsigned coverage[384][496];
int main(void) {
    Ps3TilePacket t={0}; t.words=words;
    for(unsigned h=0;h<512;h+=31) {
        memset(words,0,sizeof words);
        for(unsigned l=1;l<4;++l) words[0x1004+l]=0x8000;
        words[0x1000]=h; words[0x1004]=499;
        if(!prepare_tile_spans(&t))return 1;
        for(unsigned cat=0;cat<2;++cat) {
            memset(coverage,0,sizeof coverage);
            unsigned begin=cat?tile_front_first:1,end=cat?tile_rect_count:tile_front_first;
            for(unsigned i=begin;i<end;++i) {
                Ps3TileRect *r=&tile_rects[i];
                if(r->sx<0||r->sx+r->w>512||r->sy<0||r->sy+r->h>512)return 2;
                for(int y=r->y;y<r->y+r->h;++y)for(int x=r->x;x<r->x+r->w;++x) {
                    if(r->sx+x-r->x!=((x-(int)h)&511))return 3;
                    if(r->sy+y-r->y!=((y+499)&511))return 4;
                    ++coverage[y][x];
                }
            }
            for(int y=0;y<384;++y)for(int x=0;x<496;++x)if(coverage[y][x]!=1)return 5;
        }
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            exe = pathlib.Path(folder)/'spans'
            subprocess.run(['cc','-O2','-x','c','-','-I',str(pathlib.Path(__file__).resolve().parent),'-o',str(exe)],input=source,text=True,check=True)
            subprocess.run([str(exe)],check=True)
