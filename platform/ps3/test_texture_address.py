"""Synthetic packed-sheet addressing checks; no game assets required."""
import math
import unittest


class TextureAddressTests(unittest.TestCase):
    def test_upload_pair_covers_exact_packed_source_block(self):
        for pair in (0, 1, 511, 1023, 2047):
            addresses = set()
            for y in (pair*2, pair*2+1):
                for x in range(1024):
                    offset = y//2*512+x//2
                    byte = ((offset>>1)&0x7ffff)*4+(offset&1)*2+(0 if y&1 else 1)
                    addresses.add(byte)
            self.assertEqual(addresses, set(range(pair*1024,(pair+1)*1024)))

    def test_decoded_atlas_matches_packed_address(self):
        for bx in (0, 32, 992, 2016):
            for by in (0, 992):
                for x in range(0, 4096, 31):
                    for y in (0, 1, 1023, 2048, 4095):
                        px, py = bx+x, by+y
                        if px >= 1024:
                            px -= 1024
                            py ^= 1024
                        off = py//2*512 + px//2
                        old_word = (off//2) & 0x7ffff
                        old_shift = (off & 1)*16 + (0 if y & 1 else 8) + (0 if x & 1 else 4)
                        carry = px//1024
                        ax, ay = px-carry*1024, (py+carry*2) % 4096
                        new_off = ay//2*512 + ax//2
                        new_word = new_off//2
                        new_shift = (new_off & 1)*16 + (0 if ay & 1 else 8) + (0 if ax & 1 else 4)
                        self.assertEqual((old_word, old_shift), (new_word, new_shift))

    def test_shader_byte_and_nibble_selection(self):
        # Sweep both parity bits, sheet crossings, and wrapping past physical RAM.
        for base_x in (0, 32, 992, 1024, 2016):
            for base_y in (0, 992):
                for x in (0, 1, 2, 31, 32, 1023, 2047, 4095):
                    for y in (0, 1, 2, 31, 1023, 4095):
                        xx, yy = base_x + x, base_y + y
                        if xx >= 1024:
                            xx -= 1024
                            yy ^= 1024
                        offset = (yy // 2) * 512 + xx // 2
                        index = (offset >> 1) & 0x7ffff
                        word = (index * 2654435761) & 0xffffffff
                        shift = (16 if offset & 1 else 0) + (0 if y & 1 else 8) + (0 if x & 1 else 4)
                        expected = (word >> shift) & 15
                        rgba = [(word >> n) & 255 for n in (0, 8, 16, 24)]
                        high = (offset * .5) % 1 > .25
                        odd = (y * .5) % 1 > .25
                        byte = rgba[(2 if odd else 3) if high else (0 if odd else 1)]
                        actual = byte % 16 if x & 1 else math.floor(byte / 16)
                        self.assertEqual(expected, actual)
                        sx, sy = index % 1024, index // 1024
                        self.assertEqual(index, sy * 1024 + sx)
                        self.assertLess(sy + 512, 1024)


if __name__ == '__main__':
    unittest.main()
