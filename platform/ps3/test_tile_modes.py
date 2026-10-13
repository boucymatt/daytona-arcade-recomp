import math
import random
import unittest


class TileModes(unittest.TestCase):
    def test_float_addressing_against_integer_rules(self):
        r = random.Random(50)
        mod = lambda x, n: x-math.floor(x/n)*n
        bit = lambda x, n: mod(math.floor(x/n), 2)
        for _ in range(20000):
            l = r.choice((0, 2))
            v, h, x, y = r.randrange(32768), r.randrange(65536), r.randrange(496), r.randrange(384)
            for mode in (1, 2, 3):
                if mode == 1:
                    expected = l if (-v & 512) else l ^ 1
                    if y >= (-v & 511): expected ^= 1
                    actual = l + 1-bit(-v,512)
                    if y >= mod(-v,512): actual = l+1-mod(actual,2)
                else:
                    expected = l if h & 512 else l ^ 1
                    if x >= (h & 511): expected ^= 1
                    actual = l+1-bit(h,512)
                    if x >= mod(h,512): actual = l+1-mod(actual,2)
                self.assertEqual(actual, expected)
                self.assertEqual(mod(x-(h&511),512), (x-(h&511))&511)
                self.assertEqual(mod(y+mod(v,512),512), (y+(v&511))&511)
            mask = r.randrange(65536)
            self.assertEqual(bit(mask, 2**(15-((x//8)%16))), bool(mask & (0x8000 >> ((x>>3)&15))))
