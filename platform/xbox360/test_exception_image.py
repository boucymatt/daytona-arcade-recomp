import struct
import unittest
from check_exception_image import validate


def fixture():
    data = bytearray(0x800)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<HH', data, 0x84, 0x1f2, 3)
    struct.pack_into('<H', data, 0x94, 224)
    struct.pack_into('<H', data, 0x98, 0x10b)
    struct.pack_into('<I', data, 0x98 + 28, 0x82000000)
    struct.pack_into('<I', data, 0x98 + 36, 0x200)
    struct.pack_into('<II', data, 0x98 + 56, 0x40000, 0x200)
    frames = bytes.fromhex('0000001000000000017a5200047c4101000c0100'
                           '0000001400000018820100000000010000430e6011417f00'
                           '00000000')
    for i, (name, size) in enumerate(((b'.text', 256), (b'.eh_fram', len(frames)), (b'.gcc_exc', 4))):
        at = 0x178 + 40 * i
        data[at:at + 8] = name.ljust(8, b'\0')
        struct.pack_into('<IIII', data, at + 8, size, (i + 1) * 0x10000, 512, (i + 1) * 512)
    data[0x400:0x400 + len(frames)] = frames
    symbols = {'__eh_frame_start': 0x82020000, '__eh_frame_end': 0x82020000 + len(frames),
               '__eh_frame_hdr_start': 0x82030000, '__eh_frame_hdr_end': 0x82030000}
    for name in ('main', '_Z19run_exception_casesv', '_ZL6nestedi', '_ZL11raise_errori',
                 '__cxa_throw', '_Unwind_RaiseException'):
        symbols[name] = 0x82010010
    return data, ''.join(f' 0001:00000000 {name} {address:016x} object\n' for name, address in symbols.items())


class ExceptionImageTest(unittest.TestCase):
    def test_valid_big_endian_frames(self):
        self.assertEqual(validate(*fixture()), (1, 1, 0))

    def test_marker_truncation(self):
        data, symbols = fixture()
        with self.assertRaisesRegex(ValueError, 'markers'):
            validate(data, symbols.replace('0000000082020030', '0000000082020020'))

    def test_bad_code_pointer(self):
        data, symbols = fixture()
        struct.pack_into('>I', data, 0x400 + 28, 0x81000000)
        with self.assertRaisesRegex(ValueError, 'code range'):
            validate(data, symbols)

    def test_discarded_record_cannot_satisfy_coverage(self):
        data, symbols = fixture()
        struct.pack_into('>I', data, 0x400 + 28, 0)
        with self.assertRaisesRegex(ValueError, 'Missing unwind coverage'):
            validate(data, symbols)

    def test_broken_cie_reference(self):
        data, symbols = fixture()
        struct.pack_into('>I', data, 0x400 + 24, 123)
        with self.assertRaisesRegex(ValueError, 'missing CIE'):
            validate(data, symbols)

    def test_extra_runtime_coverage_required(self):
        data, symbols = fixture()
        symbols += ' 0001:00000000 runtime_service 0000000082011000 object\n'
        with self.assertRaisesRegex(ValueError, 'Missing unwind coverage: runtime_service'):
            validate(data, symbols, ('runtime_service',))


if __name__ == '__main__':
    unittest.main()
