"""Exercise actual PPC COFF relocation output beyond the old 8 MiB limit."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdk', type=Path, required=True)
    ap.add_argument('--linker', type=Path, required=True)
    args = ap.parse_args()
    with tempfile.TemporaryDirectory(prefix='xbox-rel24-') as tmp:
        out = Path(tmp)
        for span, unaligned, valid in ((9 * 1024 * 1024, False, True),
                                       (32 * 1024 * 1024, False, False),
                                       (16, True, False)):
            (out / 'start.s').write_text(f'.text\n.globl _start\n_start:\nbl destination\nblr\n.space {span}\n')
            (out / 'end.s').write_text('.text\n' + ('.byte 0\n' if unaligned else '') +
                                       '.globl destination\ndestination:\nbl _start\nblr\n')
            for name in ('start', 'end'):
                subprocess.run([str(args.sdk / 'bin/clang'), '--target=ppc32-xbox360',
                                '-c', str(out / (name + '.s')), '-o', str(out / (name + '.obj'))], check=True)
            result = subprocess.run([str(args.linker), '/SUBSYSTEM:xbox360', '/FIXED', '/BASE:0x82000000',
                                     '/ALIGN:0x10000', '/ENTRY:_start', '/NODEFAULTLIB', '/OPT:NOREF',
                                     '/OUT:' + str(out / 'branch.exe'), str(out / 'start.obj'), str(out / 'end.obj')],
                                    capture_output=True, text=True)
            if not valid:
                if result.returncode == 0 or 'out of range or unaligned' not in result.stderr:
                    raise RuntimeError('Invalid branch was not rejected: ' + result.stderr)
                continue
            if result.returncode:
                raise RuntimeError(result.stderr)
            image = (out / 'branch.exe').read_bytes()
            pe = struct.unpack_from('<I', image, 60)[0]
            sections = struct.unpack_from('<H', image, pe + 6)[0]
            table = pe + 24 + struct.unpack_from('<H', image, pe + 20)[0]
            text = None
            for i in range(sections):
                at = table + i * 40
                if image[at:at + 8].rstrip(b'\0') == b'.text':
                    offset = struct.unpack_from('<I', image, at + 20)[0]
                    size = struct.unpack_from('<I', image, at + 16)[0]
                    text = image[offset:offset + size]
            if text is None:
                raise RuntimeError('No linked code section')
            def displacement(at):
                insn = struct.unpack_from('>I', text, at)[0]
                if insn & 0xfc000003 != 0x48000001:
                    raise RuntimeError('Not a relative branch-with-link')
                value = insn & 0x03fffffc
                return value - (1 << 26) if value & (1 << 25) else value
            dest = displacement(0)
            if dest < span or dest + displacement(dest) != 0:
                raise RuntimeError('Forward/backward branch target mismatch')
        print('PPC REL24: >8 MiB forward/backward targets verified; overflow/alignment rejected')


if __name__ == '__main__':
    main()
