import pathlib
import subprocess
import tempfile
import unittest

class MaterialCache(unittest.TestCase):
    def test_reuse_collisions_and_frame_invalidation(self):
        code = r'''
#include "material_cache.h"
int main() {
    static uint8_t luma[131072], xlat[49152];
    for(unsigned i=0;i<sizeof(luma);++i) luma[i]=i*7;
    for(unsigned i=0;i<sizeof(xlat);++i) xlat[i]=i*13;
    Ps3MaterialCache cache;
    cache.begin();
    auto row=cache.get(luma,xlat,123,5,200);
    if(cache.get(luma,xlat,123,5,200)!=row || cache.colours.size()!=16) return 1;
    // Exceed the hash slot count; earlier rows must remain intact.
    for(unsigned c=0;c<8192;++c) {
        auto r=cache.get(luma,xlat,c,5,200);
        for(unsigned n=0;n<16;++n)
            if(cache.colours[r*16+n]!=ps3_material_colour(luma,xlat,c,640,200,n)) return 2;
    }
    for(unsigned n=0;n<16;++n)
        if(cache.colours[row*16+n]!=ps3_material_colour(luma,xlat,123,640,200,n)) return 3;
    cache.begin();
    for(auto &v:xlat) v=255;
    row=cache.get(luma,xlat,123,5,200);
    if(row!=0 || cache.colours[0]!=0xffffffffu) return 4;
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            exe=pathlib.Path(folder)/'cache'
            subprocess.run(['g++','-std=c++20','-O2','-x','c++','-',
                            '-I',str(pathlib.Path(__file__).resolve().parent),'-o',str(exe)],
                           input=code,text=True,check=True)
            subprocess.run([str(exe)],check=True)
