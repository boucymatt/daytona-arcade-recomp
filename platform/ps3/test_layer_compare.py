import pathlib
import subprocess
import tempfile
import unittest


class LayerCompareTest(unittest.TestCase):
    def test_all_pixels_and_bits(self):
        root = pathlib.Path(__file__).resolve().parent
        source = r'''
#include "layer_compare.h"
int main(void) {
    uint32_t a[496], b[496];
    for (unsigned i=0;i<496;++i) a[i]=b[i]=i*2654435761u;
    if (!ps3_layer_row_equal(a,b)) return 1;
    for (unsigned i=0;i<496;++i) for (unsigned bit=0;bit<32;++bit) {
        b[i]^=1u<<bit;
        if (ps3_layer_row_equal(a,b)) return 2;
        b[i]^=1u<<bit;
    }
    return !ps3_layer_row_equal(a,b);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            exe = pathlib.Path(folder) / 'test'
            subprocess.run(['cc', '-O2', '-x', 'c', '-', '-I', str(root), '-o', str(exe)],
                           input=source, text=True, check=True)
            subprocess.run([str(exe)], check=True)
