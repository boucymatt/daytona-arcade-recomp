#!/usr/bin/env python3
"""Apply narrow experimental exception-port patches to pinned external LLVM.

Does not replace the installed SDK. Build/compiler validation is a separate
gate from an exception runtime link or console execution.
"""
import argparse
from pathlib import Path
import subprocess

PIN = "890b83f6c8259a8899e182a5f7d9cf39c64131cc"


def patched(relative, original):
    if relative == 'lld/COFF/Chunks.cpp':
        old = '''  if (!isInt<24>(newOffset))
    error("PPC32 BL relocation doesnt fit into 24 bit range!");'''
        new = '''  // LI is 24 bits in words: signed byte displacement is 26 bits.
  // Match the ELF PowerPC REL24 check; reject unaligned destinations too.
  if (!isInt<26>(newOffset) || (newOffset & 3))
    error("PPC32 BL relocation is out of range or unaligned");'''
        if original.count(old) != 1:
            raise ValueError(f'Unexpected patch context: {relative}')
        return original.replace(old, new)
    if relative.endswith('PPCCOFFStreamer.cpp'):
        old = '      : MCWinCOFFStreamer(C, std::move(AB), std::move(CE), std::move(OW)) {}'
        new = old + '''
  void finishImpl() override {
    emitFrames(nullptr);
    MCWinCOFFStreamer::finishImpl();
  }'''
        if original.count(old) != 1:
            raise ValueError(f'Unexpected patch context: {relative}')
        return original.replace(old, new)
    if relative.endswith('PPCAsmPrinter.cpp'):
        begin = original.index('void PPCXbox360AsmPrinter::emitTTypeReference(')
        end = original.index('// Force static initialization.', begin)
        return original[:begin] + '''void PPCXbox360AsmPrinter::emitTTypeReference(const GlobalValue *GV,
                                              unsigned Encoding) {
  // Xbox uses PE/COFF, not an AIX XCOFF TOC. Delegate encoded references
  // to the format-aware common implementation.
  AsmPrinter::emitTTypeReference(GV, Encoding);
}

''' + original[end:]
    if relative.endswith('config.h'):
        old = '#elif defined(_WIN32)\n#if defined(__MINGW32__)'
        new = '#elif defined(_WIN32) || defined(_XBOX)\n#if defined(__MINGW32__)'
    else:
        old, new = '#elif defined(_WIN32)', '#elif defined(_WIN32) || defined(_XBOX)'
    if original.count(old) != 1:
        raise ValueError(f'Unexpected patch context: {relative}')
    return original.replace(old, new)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='OpenXeChain buildscript source directory')
    args = parser.parse_args()
    source = args.source.resolve() / 'llvm'
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != PIN:
        parser.error('Unsupported LLVM revision')
    changes = []
    for relative in ('llvm/lib/Target/PowerPC/PPCAsmPrinter.cpp',
                     'llvm/lib/Target/PowerPC/MCTargetDesc/PPCCOFFStreamer.cpp',
                     'libunwind/src/config.h', 'libunwind/src/assembly.h',
                     'lld/COFF/Chunks.cpp'):
        original = subprocess.check_output(['git', '-C', str(source), 'show', f'HEAD:{relative}'], text=True)
        updated = patched(relative, original)
        path = source / relative
        if path.read_text() not in (original, updated):
            parser.error(f'Refusing to overwrite unrelated modifications: {path}')
        changes.append((path, updated))
    for path, updated in changes:
        if path.read_text() != updated:
            path.write_text(updated)
    print('Experimental compiler/unwinder source patches prepared; build and hardware validation remain')


if __name__ == '__main__':
    main()
