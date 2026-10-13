import pathlib
import subprocess
import tempfile
import unittest

class MaterialColour(unittest.TestCase):
    def test_bright_texel_uses_luminance_not_index_darkening(self):
        source = r'''
#include "material_colour.h"
int main(void) {
    for(unsigned value=0;value<256;++value) {
        unsigned reference=value>64?(unsigned)((double)(value-64)*255.0/191.0):0;
        if(ps3_gamma_byte(value)!=reference) return 4;
    }
    static uint8_t luma[131072], xlat[49152];
    for(unsigned c=0;c<3;++c) for(unsigned i=0;i<64;++i)
        xlat[(c*8192+i)*2]=(uint8_t)(64+i*3);
    luma[(128+14*8)*4]=255;
    luma[(128+1*8)*4]=0;
    if(ps3_material_colour(luma,xlat,0,128,255,14)!=0xfffcfcfc) return 1;
    if(ps3_material_colour(luma,xlat,0,128,255,1)!=0xff000000) return 2;
    if(ps3_material_colour(luma,xlat,0,128,0,14)!=0xff000000) return 3;
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            exe=pathlib.Path(folder)/'test'
            subprocess.run(['cc','-O2','-x','c','-','-I',str(pathlib.Path(__file__).resolve().parent),'-o',str(exe)],input=source,text=True,check=True)
            subprocess.run([str(exe)],check=True)
