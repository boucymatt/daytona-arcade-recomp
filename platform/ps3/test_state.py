from pathlib import Path
import struct
import tempfile
import unittest
from compare_state import read_state, SECTIONS

class StateTests(unittest.TestCase):
    def read(self, words):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'state.bin'
            path.write_bytes(struct.pack('<' + 'I' * len(words), *words))
            return read_state(path)

    def test_roundtrip(self):
        frame, sections = self.read([0x31545344, 181] + [1, 42] * 5)
        self.assertEqual(frame, 181)
        self.assertEqual(sections, {name: (42,) for name in SECTIONS})

    def test_invalid(self):
        for words in ([], [0, 181], [0x31545344, 181],
                      [0x31545344, 181, 999],
                      [0x31545344, 181] + [0] * 6):
            with self.assertRaises(ValueError):
                self.read(words)
