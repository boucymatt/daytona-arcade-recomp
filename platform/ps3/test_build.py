import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('ps3_build', Path(__file__).with_name('build.py'))
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)

class HeaderTests(unittest.TestCase):
    def fixture(self):
        data = bytearray(64)
        data[:6] = b'\x7fELF\x02\x02'
        data[18:20] = (21).to_bytes(2, 'big')
        return data

    def test_ppc64_big_endian(self):
        builder.validate_elf(self.fixture())

    def test_reject_wrong_target(self):
        for offset, value in [(0, 0), (4, 1), (5, 1), (19, 62)]:
            data = self.fixture()
            data[offset] = value
            with self.assertRaises(RuntimeError):
                builder.validate_elf(data)

    def test_reject_truncated(self):
        for size in range(64):
            with self.assertRaises(RuntimeError):
                builder.validate_elf(self.fixture()[:size])

if __name__ == '__main__':
    unittest.main()
