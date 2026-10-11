import struct
import tempfile
import unittest
from pathlib import Path
from package_image import materialize, validate_xex


def fixture():
    data = bytearray(0x600)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x1f2, 2)
    struct.pack_into('<H', data, 0x94, 224)
    opt = 0x98
    struct.pack_into('<H', data, opt, 0x10b)
    struct.pack_into('<I', data, opt + 36, 0x200)
    struct.pack_into('<II', data, opt + 56, 0x40000, 0x200)
    table = opt + 224
    struct.pack_into('<IIII', data, table + 8, 0x200, 0x10000, 0x200, 0x200)
    struct.pack_into('<IIII', data, table + 48, 0x12000, 0x20000, 0x200, 0x400)
    data[0x200:0x400] = b'C' * 0x200
    data[0x400:0x600] = b'D' * 0x200
    return data


class PackagingTest(unittest.TestCase):
    def test_materializes_bss_preserving_initialized_bytes(self):
        source = fixture()
        result = materialize(source)
        table = 0x178
        self.assertEqual(result[:table], source[:table])
        for index, pattern in enumerate((b'C', b'D')):
            entry = table + index * 40
            self.assertEqual(result[entry + 8:entry + 16], source[entry + 8:entry + 16])
            raw, offset = struct.unpack_from('<II', result, entry + 16)
            self.assertEqual(result[offset:offset + 0x200], pattern * 0x200)
            self.assertEqual(result[offset + 0x200:offset + raw], bytes(raw - 0x200))
        self.assertEqual(struct.unpack_from('<I', result, table + 56)[0], 0x20000)
        self.assertEqual(materialize(result), result)

    def test_truncated_and_overlapping_sections_rejected(self):
        for data in (fixture()[:100], fixture()[:-1], bytearray(b'garbage')):
            with self.assertRaises(ValueError):
                materialize(data)
        data = fixture()
        struct.pack_into('<I', data, 0x178 + 52, 0x10000)
        with self.assertRaises(ValueError):
            materialize(data)

    def test_pure_zero_fill_section(self):
        data = fixture()
        struct.pack_into('<II', data, 0x178 + 56, 0, 0)
        result = materialize(data)
        raw, offset = struct.unpack_from('<II', result, 0x178 + 56)
        self.assertEqual(result[offset:offset + raw], bytes(0x20000))

    def test_reject_missing_page_in_xex(self):
        with tempfile.TemporaryDirectory() as directory:
            pe, xex = Path(directory) / 'test.exe', Path(directory) / 'test.xex'
            pe.write_bytes(materialize(fixture()))
            data = bytearray(0x1000 + 0x40000)
            data[:4] = b'XEX2'
            struct.pack_into('>I', data, 8, 0x1000)
            data[0x1000:0x1002] = b'MZ'
            xex.write_bytes(data)
            validate_xex(pe, xex)
            xex.write_bytes(data[:-0x10000])
            with self.assertRaisesRegex(ValueError, 'cover all'):
                validate_xex(pe, xex)


if __name__ == '__main__':
    unittest.main()
