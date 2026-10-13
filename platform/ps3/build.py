"""Build PSL1GHT display or bounded game-runtime diagnostics, not gameplay."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

def validate_elf(data):
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x02' or int.from_bytes(data[18:20], 'big') != 21:
        raise RuntimeError('Expected big-endian ELF64 PowerPC')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=Path('/usr/local/ps3dev'))
    parser.add_argument('--check-cxx', action='store_true')
    parser.add_argument('--runtime-probe', action='store_true', help='Package C++20 runtime checks with display test')
    parser.add_argument('--game-probe', action='store_true', help='Build bounded real runtime with host oracle')
    parser.add_argument('--game-display', action='store_true', help='Present 600 checked game frames through RSX')
    parser.add_argument('--performance', action='store_true', help='600 displayed frames with timing, no hash/trace overhead')
    parser.add_argument('--rsx-probe', action='store_true', help='Bounded GPU render-target clear/scissor test, no game')
    parser.add_argument('--rsx-game', action='store_true', help='Bounded real game geometry on RSX, diagnostic texture')
    parser.add_argument('--reuse-runtime-objects', action='store_true', help='Reuse unchanged runtime/generated objects for frontend-only work')
    parser.add_argument('--reuse-prepared-game', action='store_true', help='Reuse private sources/oracle for target-only build changes')
    args = parser.parse_args()
    if args.rsx_game:
        args.performance = True
    if args.rsx_probe and (args.performance or args.game_probe or args.game_display or args.runtime_probe):
        parser.error('--rsx-probe is isolated from game/runtime probes')
    if args.performance:
        args.game_display = True
    if args.game_display:
        args.game_probe = True
    sdk = args.sdk.resolve()
    source = Path(__file__).resolve().parent
    out = source.parents[1] / 'build/ps3-current'
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, PS3DEV=str(sdk), PSL1GHT=str(sdk),
               PATH=f'{sdk}/bin:{sdk}/ppu/bin:' + os.environ['PATH'])
    def run(*cmd):
        subprocess.run(list(map(str, cmd)), check=True, env=env)
    if args.check_cxx:
        run(sdk / 'ppu/bin/ppu-g++', '-std=c++20', source / 'cxx_probe.cpp',
            '-o', out / 'cxx-probe.elf', '-lrt', '-llv2')
        return
    elf = out / 'daytona-ps3-probe.elf'
    obj = out / 'main.o'
    if args.rsx_probe or args.rsx_game:
        cg = out.parent / 'ps3-cg/usr/lib64'
        env['LD_LIBRARY_PATH'] = str(cg) + ':' + env.get('LD_LIBRARY_PATH', '')
        arrays = []
        for stage, flag in [('v', '-v'), ('f', '-f')]:
            binary = out / ('probe.' + stage + 'po')
            shader = 'material' if args.rsx_game else 'probe'
            run(sdk / 'bin/cgcomp', flag, source / (shader + '.' + stage + 'cg'), binary)
            data = binary.read_bytes()
            arrays.append('static unsigned char probe_' + stage + '[] __attribute__((aligned(16))) = {'
                          + ','.join(str(b) for b in data) + '};')
        if args.rsx_game:
            binary = out / 'layers.fpo'
            run(sdk / 'bin/cgcomp', '-f', source / 'layers.fcg', binary)
            arrays.append('static unsigned char layer_f[] __attribute__((aligned(16))) = {'
                          + ','.join(str(b) for b in binary.read_bytes()) + '};')
        (out / 'probe_shaders.h').write_text('\n'.join(arrays) + '\n')
    run(sdk / 'ppu/bin/ppu-gcc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-mcpu=cell',
        '-fno-fast-math', '-I' + str(out), '-I' + str(sdk / 'ppu/include'), source / 'main.c',
        *(['-DPS3_CXX_PROBE'] if args.runtime_probe else []),
        *(['-DPS3_RSX_PROBE'] if args.rsx_probe else []),
        *(['-DPS3_GAME_PROBE'] if args.game_probe else []), '-c', '-o', obj)
    if args.game_display:
        run(sdk / 'ppu/bin/ppu-gcc', '-std=gnu11', '-O2', '-mcpu=cell',
            '-I' + str(out), '-I' + str(sdk / 'ppu/include'), '-DPS3_GAME_PROBE', '-DPS3_GAME_DISPLAY',
            *(['-DPS3_RSX_GAME'] if args.rsx_game else []),
            *(['-DPS3_PERF_PROBE'] if args.performance else []),
            '-c', source / 'main.c', '-o', obj)
    objects = [obj]
    if args.game_probe:
        sys.path.insert(0, str(source.parent / 'xbox360'))
        from prepare_game_probe import prepare_game, RUNTIME
        if args.reuse_prepared_game:
            host = out.parent / 'ps3-host-tools'
            sources = [source.parents[1] / 'src/runtime' / (n + '.cpp') for n in RUNTIME]
            generated = sorted((host / 'gen/daytona93').glob('*.cpp'))
            if not generated or not (out / 'game_expected.h').is_file() or not (out / 'game_assets.h').is_file():
                raise RuntimeError('Run --game-probe without reuse first')
            sources += generated + [host / 'gen/daytona93_tgp/tgp_gen.cpp']
        else:
            sources = prepare_game(out.parent, out, 'ps3-host-tools', 600 if args.game_display else 120, True)
        sources.append(source.parent / 'xbox360/game_probe.cpp')
        audio_units = ('native_sound_engine.cpp', 'native_sound_sequencer.cpp', 'native_sample_mixer.cpp') if args.rsx_game else ()
        sources.extend(source.parents[1] / 'src/runtime' / name for name in audio_units)
        def compile_unit(unit):
            target = out / (unit.name + '.o')
            if args.reuse_runtime_objects and unit.name not in ('game_probe.cpp', 'raster.cpp', 'geo.cpp', 'video.cpp', 'lockstep.cpp') + audio_units:
                if not target.is_file():
                    raise RuntimeError('Missing runtime object: ' + str(target))
                return target
            # Rendering-only optimization experiment; preserve strict FP and all
            # other runtime/generated compilation settings.
            optimization = '-O3' if unit.name in ('raster.cpp', 'video.cpp') else '-O2'
            run(sdk / 'ppu/bin/ppu-g++', '-std=gnu++20', optimization, '-mcpu=cell',
                *(['-funroll-loops'] if unit.name == 'raster.cpp' else []),
                '-fno-fast-math', '-ffp-contract=off', '-DM2_XBOX360_HEADLESS',
                '-DPS3_GAME_PROBE', '-I' + str(source.parents[1] / 'src'),
                *(['-DPS3_RSX_GAME'] if args.rsx_game else []),
                *(['-DPS3_PERF_PROBE'] if args.performance else ['-DM2_RASTER_DIAGNOSTICS']),
                *(['-DPS3_GAME_DISPLAY', '-DGAME_PROBE_FRAMES=' + ('36000' if args.rsx_game else '600')] if args.game_display else []),
                '-include', source / 'cxx_compat.h',
                '-I' + str(out), '-I' + str(sdk / 'ppu/include'), '-c', unit, '-o', target)
            return target
        with ThreadPoolExecutor(max_workers=4) as pool:
            objects.extend(pool.map(compile_unit, sources))
    if args.runtime_probe:
        cxx_obj = out / 'cxx_probe.o'
        run(sdk / 'ppu/bin/ppu-g++', '-std=c++20', '-O2', '-mcpu=cell', '-fno-fast-math',
            '-DPS3_CXX_PROBE', '-c', source / 'cxx_probe.cpp', '-o', cxx_obj)
        objects.append(cxx_obj)
    run(sdk / ('ppu/bin/ppu-g++' if args.runtime_probe or args.game_probe else 'ppu/bin/ppu-gcc'),
        *objects, '-L' + str(sdk / 'ppu/lib'), '-lrsx', '-lgcm_sys', '-lio', '-lsysutil',
        *(['-laudio'] if args.rsx_game else []), '-lrt', '-llv2', '-lm', '-o', elf)
    run(sdk / 'bin/sprxlinker', elf)
    validate_elf(elf.read_bytes())
    run(sdk / 'bin/make_self', elf, out / 'daytona-ps3-probe.self')
    package = out / 'pkg'
    (package / 'USRDIR').mkdir(parents=True, exist_ok=True)
    content = 'UP0001-DAYTPS301_00-0000000000000000'
    run(sdk / 'bin/make_self_npdrm', elf, package / 'USRDIR/EBOOT.BIN', content)
    run(sdk / 'bin/sfo', '--title', 'Daytona PS3 display test', '--appid', 'DAYTPS301',
        '-f', sdk / 'bin/sfo.xml', package / 'PARAM.SFO')
    run(sdk / 'bin/pkg', '--contentid', content, package, out / 'daytona-ps3-probe.pkg')
    for artifact, magic in [(out / 'daytona-ps3-probe.self', b'SCE\0'),
                            (package / 'USRDIR/EBOOT.BIN', b'SCE\0'),
                            (out / 'daytona-ps3-probe.pkg', b'\x7fPKG')]:
        if artifact.read_bytes()[:4] != magic:
            raise RuntimeError('Invalid output signature: ' + str(artifact))
    print('Built PS3 probe; this package still requires console validation:', out)

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print('PS3 build step failed; see diagnostic above.', file=sys.stderr)
        sys.exit(error.returncode)
