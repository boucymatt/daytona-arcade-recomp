import pathlib
import subprocess
import tempfile
import unittest


class TileColour(unittest.TestCase):
    def test_all_pens_categories_and_modes(self):
        source = r'''
#include "tile_colour.h"
int main(void) {
    for(unsigned p=0;p<4096;++p)for(unsigned c=0;c<2;++c)for(unsigned v=0;v<4;++v) {
        unsigned rgb=(p*7919)&0xffffff;
        int opaque=v>=2, ignore_category=v==3, target=v==1;
        int expected=(ignore_category || c==target) && (opaque || (p&15));
        uint32_t out=ps3_tile_colour(p,c,v,rgb);
        if(out!=(expected?(rgb|0xff000000u):0))return 1;
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            exe=pathlib.Path(folder)/'colour'
            subprocess.run(['cc','-O2','-x','c','-','-I',str(pathlib.Path(__file__).resolve().parent),'-o',str(exe)],input=source,text=True,check=True)
            subprocess.run([str(exe)],check=True)
