"""Compare private DST1 frame dumps without printing game memory contents."""
import argparse
from pathlib import Path
import struct

SECTIONS = ('cpu', 'main_ram', 'work_ram', 'video', 'tgp_count')

def read_state(path):
    data = Path(path).read_bytes()
    if len(data) < 8 or len(data) % 4:
        raise ValueError('Truncated state dump')
    words = struct.unpack('<' + 'I' * (len(data) // 4), data)
    if words[0] != 0x31545344:
        raise ValueError('Not a DST1 dump')
    pos, result = 2, {}
    for name in SECTIONS:
        if pos >= len(words):
            raise ValueError('Missing section: ' + name)
        count = words[pos]
        pos += 1
        if count > len(words) - pos:
            raise ValueError('Truncated section: ' + name)
        result[name] = words[pos:pos + count]
        pos += count
    if pos != len(words):
        raise ValueError('Trailing state data')
    return words[1], result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('ps3')
    args = parser.parse_args()
    frame, host = read_state(args.host)
    other_frame, target = read_state(args.ps3)
    if frame != other_frame:
        raise ValueError('Frame numbers differ')
    print('Frame', frame)
    for name in SECTIONS:
        a, b = host[name], target[name]
        if len(a) != len(b):
            print(name, 'length mismatch', len(a), len(b))
            continue
        mismatches = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
        print(name, 'PASS' if not mismatches else
              f'{len(mismatches)} differing words; first word indices {mismatches[:8]}')

if __name__ == '__main__':
    main()
