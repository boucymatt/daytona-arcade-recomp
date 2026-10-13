"""Compare selected pixel calculations in private host and PS3 logs."""
import argparse
from pathlib import Path

FIELDS = ('start_z', 'start_u', 'start_v', 'step_z', 'step_u', 'step_v',
          'ooz', 'uoz', 'voz', 'z', 'u', 'v', 'mml', 'level', 'texel',
          'lumabase', 'luma', 'colorbase', 'red_base', 'green_base', 'blue_base',
          'pixel', 'x0', 'x1', 'translucent', 'texx', 'texy', 'texwidth',
          'texheight', 'texlod')

def records(path):
    result = []
    for line in Path(path).read_text().splitlines():
        if not line.startswith('RasterPixel '):
            continue
        parts = line.split()
        if len(parts) != len(FIELDS) + 3:
            raise ValueError('Wrong raster diagnostic field count')
        result.append(((int(parts[1]), int(parts[2])), tuple(int(v, 16) for v in parts[3:])))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('ps3')
    args = parser.parse_args()
    host, target = records(args.host), records(args.ps3)
    if not host or not target:
        raise ValueError('Missing raster diagnostics')
    print('Records:', len(host), len(target))
    for n, ((xy, a), (other_xy, b)) in enumerate(zip(host, target)):
        if xy != other_xy:
            print('Draw order differs at record', n, xy, other_xy)
            break
        changes = [(name, x, y) for name, x, y in zip(FIELDS, a, b) if x != y]
        print(xy, 'PASS' if not changes else 'DIFF')
        for name, x, y in changes:
            print(f'  {name}: host={x:08x} ps3={y:08x}')
    def geometry(path):
        rows = []
        for line in Path(path).read_text().splitlines():
            if line.startswith('RasterGeometry '):
                parts = line.split()
                rows.append((tuple(map(int, parts[1:4])), tuple(int(v, 16) for v in parts[4:])))
        return rows
    ga, gb = geometry(args.host), geometry(args.ps3)
    print('Geometry records:', len(ga), len(gb))
    for (key, a), (other, b) in zip(ga, gb):
        if key != other or len(a) != len(b):
            print('Geometry layout/order differs:', key, other)
            break
        changes = [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y]
        print('Geometry', key, 'PASS' if not changes else 'DIFF')
        for i, x, y in changes:
            print(f'  word {i}: host={x:08x} ps3={y:08x}')

if __name__ == '__main__':
    main()
