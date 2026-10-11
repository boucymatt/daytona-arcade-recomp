"""Instrument the existing failing game-probe objects, without regenerating them.

This intentionally reuses the previous diagnostic runtime/generated objects.
The runner (and optionally video.cpp) is replaced. The manifest records which baseline
was instrumented. It is not a general incremental game build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from prepare_game_probe import RUNTIME
from check_exception_image import validate
from package_image import prepare, validate_xex


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdk', type=Path, required=True)
    ap.add_argument('--build-root', type=Path, default=Path('build'))
    ap.add_argument('--small-draw-stack', action='store_true', help='Rebuild video with the heap tile mask for longer startup comparisons')
    modes = ap.add_mutually_exclusive_group()
    modes.add_argument('--startup', action='store_true', help='Run all 120 reference frames without instruction tracing')
    modes.add_argument('--second-frame', action='store_true', help='Trace only frame 1 after an unchanged frame 0')
    modes.add_argument('--quiet-second-frame', action='store_true', help='Same second-frame slicing without checkpoint writes')
    modes.add_argument('--paced-second-frame', action='store_true', help='Quiet slices with native 1 ms delays and a 30-second exit observation')
    modes.add_argument('--paced-startup', action='store_true', help='120 paced frames, then park without automatic dashboard return')
    modes.add_argument('--trace-fourth-frame', action='store_true', help='Keep startup pacing, trace frame 3, then park')
    modes.add_argument('--trace-video', action='store_true', help='Trace frame 3 and rebuild video with composition checkpoints')
    args = ap.parse_args()
    if args.trace_video:
        args.trace_fourth_frame = True
    paced_startup = args.paced_startup or args.trace_fourth_frame
    second_frame = args.second_frame or args.quiet_second_frame or args.paced_second_frame
    repo = Path(__file__).resolve().parents[2]
    root, sdk = args.build_root.resolve(), args.sdk.resolve()
    baseline = root / 'xbox360-game'
    # Reuse one diagnostic output directory; baseline objects/ROMs stay separate.
    out = root / 'xbox360-current'
    trace_flags = (['-DXBOX_GAME_TRACE', '-DXBOX_GAME_SECOND_FRAME'] if second_frame
                   else [] if args.startup else ['-DXBOX_GAME_TRACE'])
    if args.quiet_second_frame or args.paced_second_frame or paced_startup:
        trace_flags.append('-DXBOX_GAME_QUIET_SLICE')
    if args.paced_second_frame or paced_startup:
        trace_flags.append('-DXBOX_GAME_PACED_SLICE')
    if paced_startup:
        trace_flags.append('-DXBOX_GAME_PACED_STARTUP')
    if args.trace_fourth_frame:
        trace_flags.append('-DXBOX_GAME_FOURTH_FRAME')
    if args.trace_video:
        trace_flags.append('-DM2_XBOX360_VIDEO_TRACE')
    if args.trace_video or args.small_draw_stack:
        trace_flags.append('-DM2_XBOX360_SMALL_DRAW_STACK')
    frame_count = 4 if args.trace_fourth_frame else 2 if second_frame else 120 if args.startup or paced_startup else 1
    runtime = root / 'xbox360-exception-runtime'
    compiler = root / 'xbox360-llvm-eh/bin/clang'
    linker = root / 'xbox360-llvm-eh/bin/lld-link'
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    if digest(compiler) != (runtime / 'compiler.sha256').read_text():
        raise RuntimeError('Compiler/runtime fingerprint mismatch')
    generated = root / 'xbox360-host-tools/gen'
    sources = [repo / 'src/runtime' / (name + '.cpp') for name in RUNTIME]
    sources += sorted((generated / 'daytona93').glob('*.cpp'))
    sources += [generated / 'daytona93_tgp/tgp_gen.cpp']
    if len(sources) < 12:
        raise RuntimeError('Missing original generated sources')
    names = ['eh_start.S', 'exception_cases.cpp', 'exception_alloc.c', 'tls_runtime.cpp']
    names += [p.name for p in sources]
    names += ['eh_end.S']
    objects = [baseline / (name + '.obj') for name in names]
    manifest = {str(p.relative_to(root)): digest(p) for p in objects}
    manifest['baseline_xex'] = digest(baseline / 'default.xex')
    out.mkdir(parents=True, exist_ok=True)
    (out / 'baseline.json').write_text(json.dumps(manifest, indent=2) + '\n')
    runner = repo / 'platform/xbox360/game_probe.cpp'
    oracle = out / 'host-trace'
    # O0 avoids an expensive optimisation pass over unchanged generated code.
    subprocess.run(['clang++', '-std=c++20', '-O0', '-g', '-fsanitize=address,undefined',
                    '-fno-fast-math', '-ffp-contract=off', '-DM2_XBOX360_HEADLESS',
                    *trace_flags, '-DXBOX_GAME_HOST_TEST', '-I', str(repo / 'src'),
                    '-I', str(baseline), str(runner), *map(str, sources), '-o', str(oracle)], check=True)
    with (out / 'host-trace.log').open('w') as log:
        subprocess.run([str(oracle), str(baseline / 'roms'), str(out / 'trace_expected.h')],
                       stdout=log, stderr=log, check=True,
                       env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                            'UBSAN_OPTIONS': 'halt_on_error=1'})
    diagnostics = (out / 'host-trace.log').read_text()
    if (args.paced_second_frame or paced_startup) and 'Mode: paced slices (1 ms); 30-second exit observation' not in diagnostics:
        raise RuntimeError('Paced diagnostic mode marker missing')
    if args.quiet_second_frame or args.paced_second_frame or args.paced_startup:
        marker = 'Mode: quiet startup slicing' if args.paced_startup else 'Mode: quiet second-frame slicing'
        if marker not in diagnostics or any(
                text in diagnostics for text in ('Before has_code', 'Before generated dispatch', 'Signature complete')):
            raise RuntimeError('Quiet slicing mode still emits checkpoints or is missing its mode marker')
    if args.trace_fourth_frame:
        before, separator, after = diagnostics.partition('Begin frame 3 ')
        if not separator or 'Before generated dispatch' in before or 'Before generated dispatch' not in after:
            raise RuntimeError('Fourth-frame tracing is missing or affects earlier frames')
    if any(s in diagnostics for s in ('runtime error:', 'ERROR: AddressSanitizer', 'SUMMARY:')):
        raise RuntimeError('Host sanitizer diagnostics; refusing Xbox package')
    hashes = lambda p: re.findall(r'0x[0-9a-f]+ull', p.read_text())
    if hashes(out / 'trace_expected.h') != hashes(baseline / 'game_expected.h')[:frame_count]:
        raise RuntimeError('Diagnostic execution changed the reference signatures')
    obj = out / 'game_probe.cpp.obj'
    compile_command = [str(compiler), '--config=' + str(sdk / 'bin/clang++.cfg'),
                    '--target=ppc32-xbox360', '-O2', '-funwind-tables', '-fno-function-sections',
                    '-fno-data-sections', '-fno-fast-math', '-ffp-contract=off', '-std=c++20',
                    '-fdwarf-exceptions', '-femulated-tls', '-DM2_XBOX360_HEADLESS', *trace_flags,
                    '-nostdinc++', '-I', str(runtime / 'include/c++/v1'), '-I', str(repo / 'src'),
                    '-I', str(baseline)]
    subprocess.run([*compile_command, '-c', str(runner), '-o', str(obj)], check=True)
    if args.trace_video or args.small_draw_stack:
        video_obj = out / 'video.cpp.obj'
        subprocess.run([*compile_command, '-c', str(repo / 'src/runtime/video.cpp'), '-o', str(video_obj)], check=True)
        objects = [video_obj if p.name == 'video.cpp.obj' else p for p in objects]
        if args.trace_video and 'Video foreground complete' not in diagnostics:
            raise RuntimeError('Host video checkpoints missing')
        manifest['instrumented_video_object'] = digest(video_obj)
        manifest['instrumented_video_source'] = digest(repo / 'src/runtime/video.cpp')
    manifest['instrumented_runner_object'] = digest(obj)
    manifest['instrumented_runner_source'] = digest(runner)
    (out / 'baseline.json').write_text(json.dumps(manifest, indent=2) + '\n')
    pe, mp = out / 'trace.exe', out / 'trace.map'
    command = [str(sdk / 'bin/clang++'), '--target=ppc32-xbox360', '-B', str(linker.parent),
               str(objects[0]), str(obj), *map(str, objects[1:-1]),
               *[str(runtime / 'lib' / name) for name in ('libc++.a', 'libc++abi.a', 'libunwind.a')],
               str(objects[-1]), '-Wl,/defaultlib:libm.a',
               '-Wl,/opt:noicf,/opt:noref,/map:' + str(mp), '-o', str(pe)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (out / 'link.log').write_text(result.stdout)
    result.check_returncode()
    if any(s in result.stdout.lower() for s in ('unsupported', 'unimplemented')):
        raise RuntimeError(result.stdout)
    print('Unwind validation:', validate(pe.read_bytes(), mp.read_text(),
          ('_ZN2rt3Cpu5resetEv', '_ZN2rt8Lockstep5applyEv')))
    prepare(pe, out / 'package.exe')
    subprocess.run([str(sdk / 'bin/synthxex'), '-i', str(out / 'package.exe'),
                    '-o', str(out / 'default.xex'), '-t', 'title'], check=True)
    validate_xex(out / 'package.exe', out / 'default.xex')
    print('Built', out / 'default.xex', 'hardware execution remains untested')


if __name__ == '__main__':
    main()
