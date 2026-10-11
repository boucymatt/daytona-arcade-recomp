#!/usr/bin/env python3
"""Build the isolated exception test; successful packaging is not a hardware pass."""
import argparse
import hashlib
from pathlib import Path
import subprocess
from check_exception_image import validate
from package_image import prepare, validate_xex
from build_fp import prepare_fp
from prepare_game_probe import prepare_game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--build-root', type=Path, default=Path('build'))
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--runtime-probe', action='store_true', help='Link real CPU services, libc++ and TLS')
    modes.add_argument('--fp-probe', action='store_true', help='Compare big-endian SoftFloat to a native oracle')
    modes.add_argument('--board-probe', action='store_true', help='Test the real board with synthetic ROM images')
    modes.add_argument('--render-probe', action='store_true', help='Compare synthetic CPU rendering to a host oracle')
    modes.add_argument('--game-probe', action='store_true', help='Compare 120 real daytona93 frames to a host oracle')
    args = parser.parse_args()
    sdk, root = args.sdk.resolve(), args.build_root.resolve()
    compiler = root / 'xbox360-llvm-eh/bin/clang'
    linker = root / 'xbox360-llvm-eh/bin/lld-link'
    if args.game_probe and not linker.is_file():
        parser.error('Build the isolated linker stage before --game-probe')
    runtime = root / 'xbox360-exception-runtime'
    if not compiler.is_file() or not (runtime / 'compiler.sha256').is_file():
        parser.error('Build the exception compiler and runtime stages first')
    if (runtime / 'compiler.sha256').read_text() != hashlib.sha256(compiler.read_bytes()).hexdigest():
        parser.error('Compiler changed: rebuild exception runtime first')
    integrated = args.runtime_probe or args.fp_probe or args.board_probe or args.render_probe or args.game_probe
    if integrated:
        subprocess.run(['cmake', '--build', str(runtime), '--target', 'cxx_static', '--parallel', '4'], check=True)
    out = root / ('xbox360-game' if args.game_probe else 'xbox360-render' if args.render_probe else 'xbox360-board' if args.board_probe else 'xbox360-fp' if args.fp_probe else 'xbox360-runtime' if args.runtime_probe else 'xbox360-exception')
    out.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parent
    if args.render_probe:
        oracle = out / 'host-oracle'
        subprocess.run(['clang++', '-std=c++20', '-O2', '-fno-fast-math', '-ffp-contract=off',
                        '-DXBOX_RENDER_HOST_TEST', '-I', str(source.parents[1] / 'src'),
                        str(source / 'render_probe.cpp'), str(source.parents[1] / 'src/runtime/raster.cpp'),
                        '-o', str(oracle)], check=True)
        (out / 'render_expected.h').write_text(subprocess.check_output([str(oracle)], text=True))
    fp_libraries = prepare_fp(sdk, root, compiler, runtime, out) if args.fp_probe else []
    objects = []
    names = ['eh_start.S', 'exception_probe.cpp', 'exception_cases.cpp', 'exception_alloc.c', 'eh_end.S']
    if args.runtime_probe:
        names[1] = 'runtime_probe.cpp'
        names[-1:-1] = ['../../src/runtime/cpu.cpp', 'tls_runtime.cpp']
    if args.fp_probe:
        names[1] = 'fp_probe.cpp'
        names[-1:-1] = ['tls_runtime.cpp']
    if args.board_probe:
        names[1] = 'board_probe.cpp'
        names[-1:-1] = ['board_probe_guards.cpp', 'tls_runtime.cpp',
            *['../../src/runtime/' + name + '.cpp' for name in
              ('cpu', 'm2_board', 'm2_tgp_board', 'tgp', 'geo', 'video', 'raster')]]
    if args.render_probe:
        names[1] = 'render_probe.cpp'
        names[-1:-1] = ['tls_runtime.cpp', '../../src/runtime/raster.cpp']
    if args.game_probe:
        names[1] = 'game_probe.cpp'
        names[-1:-1] = ['tls_runtime.cpp', *map(str, prepare_game(root, out))]
    for name in names:
        obj = out / (Path(name).name + '.obj')
        flags = ['-O2', '-funwind-tables', '-fno-function-sections', '-fno-data-sections',
                 '-fno-fast-math', '-ffp-contract=off']
        if name.endswith('.cpp'):
            flags += ['-std=c++20', '-fdwarf-exceptions']
            if args.render_probe or args.game_probe:
                flags += ['-I', str(out)]
            if args.game_probe:
                flags += ['-DM2_XBOX360_HEADLESS']
            if integrated:
                flags += ['-femulated-tls', '-nostdinc++', '-I', str(runtime / 'include/c++/v1'),
                          '-I', str(source.parents[1] / 'src')]
            if args.fp_probe:
                flags += ['-DSOFTFLOAT_FAST_INT64', '-DTHREAD_LOCAL=__thread',
                          '-I', str(out), '-I', str(source.parents[1] / 'extern/softfloat/source/include')]
        subprocess.run([str(compiler), '--config=' + str(sdk / 'bin/clang++.cfg'),
                        '--target=ppc32-xbox360', *flags, '-c', str(source / name), '-o', str(obj)], check=True)
        objects.append(obj)
    pe, map_file = out / 'exception.exe', out / 'exception.map'
    # End marker's $ZZZ suffix ensures placement after archive frame records.
    command = [str(sdk / 'bin/clang++'), '--target=ppc32-xbox360',
               *(['-B', str(linker.parent)] if args.game_probe else []),
               *map(str, objects[:-1]),
               *map(str, fp_libraries),
               *([str(runtime / 'lib/libc++.a')] if integrated else []),
               str(runtime / 'lib/libc++abi.a'),
               str(runtime / 'lib/libunwind.a'), str(objects[-1]),
               *(['-Wl,/defaultlib:libm.a'] if args.fp_probe or args.board_probe or args.render_probe or args.game_probe else []),
               '-Wl,/opt:noicf,/opt:noref,/map:' + str(map_file), '-o', str(pe)]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end='')
    (out / 'link.log').write_text(result.stdout)
    result.check_returncode()
    if 'unimplemented' in result.stdout.lower() or 'unsupported' in result.stdout.lower():
        raise RuntimeError('Unsupported linker operation; refusing to package')
    coverage = ('_ZN2rt10fatalerrorEPKcz', '_ZN2rt3Cpu7do_callEjij',
                '_ZN2rt3Cpu8do_ret_0Ev', '_ZN2rt3Cpu5resetEv') if args.runtime_probe else ()
    if args.fp_probe:
        coverage = ('_Z12run_fp_casesv', 'f32_to_extF80', 'extF80_to_i32')
    if args.board_probe:
        coverage = ('_ZN2rt7M2Board5fetchEj', '_ZN2rt7M2Board10read_dwordEj',
                    '_ZN2rt7M2Board11write_dwordEjj')
    if args.render_probe:
        coverage = ('_ZNK2rt6Raster4hashEiiii', '_ZN2rt6Raster10render_oneENS_7GeoPolyEiiiiiiii')
    if args.game_probe:
        coverage = ('_ZN2rt3Cpu5resetEv', '_ZN2rt8Lockstep5applyEv')
    cies, frames, discarded = validate(pe.read_bytes(), map_file.read_text(), coverage)
    print(f'Validated {cies} CIEs, {frames} live FDEs, {discarded} discarded COMDAT records')
    packed, xex = out / 'package.exe', out / 'default.xex'
    prepare(pe, packed)
    subprocess.run([str(sdk / 'bin/synthxex'), '-i', str(packed), '-o', str(xex), '-t', 'title'], check=True)
    validate_xex(packed, xex)
    print(f'Built {xex}; this probe still requires console testing')


if __name__ == '__main__':
    main()
