#!/usr/bin/env python3
"""Cross-compile Windows x64 executables using Linux MinGW-w64.

Run ./setup.sh first to fetch dependencies and generate game C++ on the host.
No Windows executables are run during this build.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-windows')
    parser.add_argument('--host-build-dir', type=Path, default=ROOT / 'build',
                        help='native setup build; Revision A uses the sibling with -daytona')
    parser.add_argument('--set', choices=('auto', 'daytona93', 'daytona'), default='auto')
    parser.add_argument('-j', '--jobs', type=int, default=min(os.cpu_count() or 1, 4))
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('this script requires a Linux host')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    for tool in ('cmake', 'ninja', 'x86_64-w64-mingw32-gcc',
                 'x86_64-w64-mingw32-g++', 'x86_64-w64-mingw32-windres'):
        if not shutil.which(tool):
            parser.error(f'{tool} missing; install CMake, Ninja and the MinGW-w64 x64 toolchain')
    for source in ('softfloat/source/include/softfloat.h', 'ymfm/src/ymfm_opn.cpp',
                   'sdl3/CMakeLists.txt', 'imgui/imgui.cpp', 'lzma/C/7zArcIn.c'):
        if not (ROOT / 'extern' / source).is_file():
            parser.error('dependencies missing; run ./setup.sh first')
    build = args.build_dir.resolve()
    host = args.host_build_dir.resolve()
    # Never reconfigure a native (or unrelated cross) build with this toolchain.
    for dest in (build, Path(str(build) + '-daytona')):
        cache = dest / 'CMakeCache.txt'
        if cache.is_file():
            toolchain = str(ROOT / 'cmake/toolchains/mingw-w64.cmake')
            entries = cache.read_text().splitlines()
            if not any(line.startswith('CMAKE_TOOLCHAIN_FILE:') and
                       line.partition('=')[2] == toolchain for line in entries):
                parser.error(f'{dest} belongs to another build; choose a fresh --build-dir')
    selected = []
    for romset in ('daytona93', 'daytona'):
        gen = (host if romset == 'daytona93' else Path(str(host) + '-daytona')) / 'gen'
        files = (gen / romset / 'gen_table.cpp', gen / f'{romset}_tgp/tgp_gen.cpp',
                 gen / f'{romset}_snd/snd_gen.cpp')
        if args.set == romset or (args.set == 'auto' and all(p.is_file() for p in files)):
            if not all(p.is_file() for p in files):
                parser.error(f'{romset}: generated code missing in {gen}; run native setup with that ROM set first')
            selected.append((romset, gen))
    if not selected:
        parser.error('no generated game code found; run ./setup.sh with ROMs in roms/ first')
    for romset, gen in selected:
        dest = build if romset == 'daytona93' else Path(str(build) + '-daytona')
        subprocess.run(['cmake', '-S', str(ROOT), '-B', str(dest), '-G', 'Ninja',
                        f'-DCMAKE_TOOLCHAIN_FILE={ROOT / "cmake/toolchains/mingw-w64.cmake"}',
                        '-DCMAKE_BUILD_TYPE=Release', '-DM2_SOFTFLOAT_PORTABLE=ON',
                        f'-DM2_ROMSET={romset}', f'-DM2_GEN_ROOT={gen}',
                        f'-DM2_GAME_DIR={build}'], check=True)
        targets = ['daytona_game']
        if romset == 'daytona93':
            targets.append('daytona_launcher')
        subprocess.run(['cmake', '--build', str(dest), '--parallel', str(args.jobs),
                        '--target', *targets], check=True)
    # A Revision A-only build still needs the shared launcher.
    if not any(romset == 'daytona93' for romset, _ in selected):
        subprocess.run(['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                        f'-DCMAKE_TOOLCHAIN_FILE={ROOT / "cmake/toolchains/mingw-w64.cmake"}',
                        '-DCMAKE_BUILD_TYPE=Release', '-DM2_SOFTFLOAT_PORTABLE=ON',
                        '-DM2_ROMSET=daytona93'], check=True)
        subprocess.run(['cmake', '--build', str(build), '--parallel', str(args.jobs),
                        '--target', 'daytona_launcher'], check=True)
    print(f'Windows executables: {build}. Start daytona.exe on Windows; import your ROM there.')


if __name__ == '__main__':
    main()
