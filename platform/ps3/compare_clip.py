"""Locate the first clipping-trace divergence in private bring-up logs."""
import argparse
from pathlib import Path

def records(path):
    result = []
    for line in Path(path).read_text().splitlines():
        if line.startswith('GeoClip '):
            parts = line.split()
            result.append((parts[1], tuple(int(x, 16) for x in parts[2:])))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('ps3')
    args = parser.parse_args()
    a, b = records(args.host), records(args.ps3)
    if not a or not b:
        raise ValueError('Missing clipping trace')
    print('Clipping records:', len(a), len(b))
    for i, (x, y) in enumerate(zip(a, b)):
        if x == y:
            continue
        print('First mismatch:', i, x[0], y[0])
        if x[0] != y[0] or len(x[1]) != len(y[1]):
            print('Record layout/order differs')
        else:
            for n, (u, v) in enumerate(zip(x[1], y[1])):
                if u != v:
                    print(f'  word {n}: host={u:08x} ps3={v:08x}')
        return
    print('PASS' if len(a) == len(b) else 'Record counts differ')

if __name__ == '__main__':
    main()
