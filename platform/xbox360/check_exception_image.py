"""Validate static big-endian DWARF frame bounds in the experimental Xbox PE.

This checks structure and linked addresses, not hardware unwinding correctness.
The LLVM COFF dump reader assumes little-endian frame data for this target.
"""
import re
import struct
from package_image import layout


def validate(image, map_text, extra_symbols=()):
    opt, table, count, _, _, _ = layout(image)
    base = struct.unpack_from('<I', image, opt + 28)[0]
    sections = {}
    for i in range(count):
        at = table + 40 * i
        name = image[at:at + 8].rstrip(b'\0').decode('ascii')
        size, rva, raw, offset = struct.unpack_from('<IIII', image, at + 8)
        if offset + raw > len(image) or size > raw:
            if name in ('.eh_fram', '.gcc_exc', '.text'):
                raise ValueError('Truncated exception/code section')
        sections[name] = (base + rva, size, image[offset:offset + min(size, raw)])
    frames_base, frame_size, frames = sections['.eh_fram']
    text_base, text_size, _ = sections['.text']
    lsda_base, lsda_size, _ = sections['.gcc_exc']

    def symbol(name):
        match = re.search(r'\s' + re.escape(name) + r'\s+([0-9a-fA-F]{16})\s', map_text)
        if not match:
            raise ValueError('Missing symbol: ' + name)
        return int(match[1], 16)

    if (symbol('__eh_frame_start'), symbol('__eh_frame_end')) != (frames_base, frames_base + frame_size):
        raise ValueError('Frame markers do not cover all records')
    if symbol('__eh_frame_hdr_start') != symbol('__eh_frame_hdr_end'):
        raise ValueError('Unexpected frame index')

    def word(at):
        if at + 4 > len(frames):
            raise ValueError('Truncated frame word')
        return struct.unpack_from('>I', frames, at)[0]

    def leb(at):
        value = shift = 0
        while at < len(frames) and shift < 35:
            byte = frames[at]
            at += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value, at
            shift += 7
        raise ValueError('Invalid LEB value')

    pos, cies, ranges, discarded = 0, {}, [], 0
    while pos < len(frames):
        size = word(pos)
        if size == 0:
            if pos != len(frames) - 4:
                raise ValueError('Premature frame terminator')
            break
        end = pos + 4 + size
        if size < 8 or end > len(frames):
            raise ValueError('Invalid frame size')
        ident = word(pos + 4)
        if ident == 0:
            at = pos + 8
            if frames[at] != 1:
                raise ValueError('Unexpected CIE version')
            stop = frames.index(0, at + 1, end)
            augmentation = frames[at + 1:stop]
            if augmentation not in (b'zR', b'zPLR'):
                raise ValueError('Unexpected CIE augmentation')
            code, at = leb(stop + 1)
            data, at = leb(at)
            if code != 4 or data != 0x7c or frames[at] != 65:
                raise ValueError('Unexpected PowerPC CIE ABI')
            length, at = leb(at + 1)
            if augmentation == b'zPLR':
                if length != 7 or frames[at] or frames[at + 5] or frames[at + 6]:
                    raise ValueError('Expected absolute CIE encodings')
                if word(at + 1) != symbol('__gxx_personality_v0'):
                    raise ValueError('Invalid personality address')
            elif length != 1 or frames[at]:
                raise ValueError('Expected absolute frame encoding')
            if at + length > end:
                raise ValueError('Truncated CIE augmentation')
            cies[pos] = augmentation
        else:
            cie = pos + 4 - ident
            if cie not in cies:
                raise ValueError('FDE references missing CIE')
            start, length = word(pos + 8), word(pos + 12)
            # LLD zeros relocations to discarded duplicate COMDAT functions.
            # Such ranges cannot match a live PC; still require live coverage
            # of each function used by this probe below.
            if start == 0 and length:
                discarded += 1
                pos = end
                continue
            if not length or not text_base <= start < start + length <= text_base + text_size:
                raise ValueError(f'Invalid FDE code range at {pos:#x}: {start:#x}+{length:#x}')
            aug_size, at = leb(pos + 16)
            if cies[cie] == b'zPLR':
                if aug_size != 4:
                    raise ValueError('Invalid LSDA encoding')
                lsda = word(at)
                if not lsda_base <= lsda < lsda_base + lsda_size:
                    raise ValueError('Invalid LSDA address')
            elif aug_size:
                raise ValueError('Unexpected FDE augmentation')
            if at + aug_size > end:
                raise ValueError('Truncated FDE')
            ranges.append((start, start + length))
        pos = end
    else:
        raise ValueError('Missing frame terminator')
    for name in ('main', '_Z19run_exception_casesv', '_ZL6nestedi', '_ZL11raise_errori',
                 '__cxa_throw', '_Unwind_RaiseException', *extra_symbols):
        address = symbol(name)
        if not any(start <= address < end for start, end in ranges):
            raise ValueError('Missing unwind coverage: ' + name)
    return len(cies), len(ranges), discarded
