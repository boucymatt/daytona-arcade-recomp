"""Materialize PE zero-fill before the pinned SynthXEX raw-section mapper.

Operate on a generated copy, never change virtual addresses or patch a signed
XEX after packaging. SynthXEX computes its page descriptors/hashes afterwards.
"""
import struct


def layout(data):
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError('Not a PE image')
    pe = struct.unpack_from('<I', data, 60)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE header')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    table = opt + optional_size
    if machine != 0x1f2 or optional_size < 96 or not count or table + count * 40 > len(data):
        raise ValueError('Expected a complete Xbox PowerPC PE32 section table')
    if struct.unpack_from('<H', data, opt)[0] != 0x10b:
        raise ValueError('Expected PE32')
    alignment = struct.unpack_from('<I', data, opt + 36)[0]
    size, headers = struct.unpack_from('<II', data, opt + 56)
    if not alignment or alignment & (alignment - 1) or not (table + count * 40 <= headers <= len(data)):
        raise ValueError('Invalid PE file alignment/header size')
    if not headers <= size <= 512 * 1024 * 1024:
        raise ValueError('Invalid Xbox image extent')
    return opt, table, count, alignment, size, headers


def materialize(data):
    opt, table, count, alignment, size, headers = layout(data)
    output = bytearray(data[:headers])
    previous_end = headers
    for i in range(count):
        entry = table + i * 40
        virtual, rva, raw, offset = struct.unpack_from('<IIII', data, entry + 8)
        extent = max(virtual, raw)
        if rva < previous_end or rva + extent > size or (raw and (offset < headers or offset + raw > len(data))):
            raise ValueError('Invalid or overlapping PE section')
        previous_end = rva + extent
        # Last section must include all trailing image pages, not just its
        # initialized bytes; earlier gaps become zeroes in SynthXEX's mapper.
        required = size - rva if i == count - 1 else extent
        raw_size = (required + alignment - 1) & ~(alignment - 1)
        if not raw_size or rva + raw_size > size:
            raise ValueError('Invalid materialized section extent')
        new_offset = (len(output) + alignment - 1) & ~(alignment - 1)
        output.extend(bytes(new_offset - len(output)))
        output.extend(data[offset:offset + raw] if raw else b'')
        output.extend(bytes(raw_size - raw))
        struct.pack_into('<II', output, entry + 16, raw_size, new_offset)
    return bytes(output)


def prepare(source, destination):
    destination.write_bytes(materialize(source.read_bytes()))


def validate_xex(pe, xex):
    expected = layout(pe.read_bytes())[4]
    data = xex.read_bytes()
    if len(data) < 24 or data[:4] != b'XEX2':
        raise ValueError('Packager output lacks a complete XEX2 header')
    offset = struct.unpack_from('>I', data, 8)[0]
    # Pinned SynthXEX emits uncompressed mapped PE payloads.
    if offset < 24 or len(data) - offset != expected:
        raise ValueError(f'XEX mapped payload must cover all {expected} PE image bytes')
    if data[offset:offset + 2] != b'MZ':
        raise ValueError('Expected uncompressed mapped PE payload')
