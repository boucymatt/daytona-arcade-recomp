import unittest
from prepare_exceptions import patched


class PatchTest(unittest.TestCase):
    def test_printer_change_is_xbox_only(self):
        original = ('prefix\nvoid PPCXbox360AsmPrinter::emitTTypeReference(const GlobalValue *GV,\n'
                    'unsigned Encoding) { old_xcoff_code(); }\n\n// Force static initialization.\nsuffix')
        output = patched('PPCAsmPrinter.cpp', original)
        self.assertTrue(output.startswith('prefix\n'))
        self.assertTrue(output.endswith('// Force static initialization.\nsuffix'))
        self.assertIn('AsmPrinter::emitTTypeReference(GV, Encoding);', output)
        self.assertNotIn('old_xcoff_code', output)

    def test_alias_patch_leaves_other_windows_checks(self):
        original = '#elif defined(_WIN32)\nother\n#elif defined(_WIN32)\n#if defined(__MINGW32__)\n'
        output = patched('config.h', original)
        self.assertEqual(output.count('defined(_XBOX)'), 1)
        self.assertTrue(output.startswith('#elif defined(_WIN32)\nother'))

    def test_unexpected_context_rejected(self):
        for path in ('assembly.h', 'config.h', 'PPCAsmPrinter.cpp', 'PPCCOFFStreamer.cpp', 'lld/COFF/Chunks.cpp'):
            with self.assertRaises(ValueError):
                patched(path, 'unrelated file')

    def test_rel24_is_signed_26_bit_aligned_byte_offset(self):
        original = '  if (!isInt<24>(newOffset))\n    error("PPC32 BL relocation doesnt fit into 24 bit range!");'
        output = patched('lld/COFF/Chunks.cpp', original)
        self.assertIn('!isInt<26>(newOffset) || (newOffset & 3)', output)


if __name__ == '__main__':
    unittest.main()
