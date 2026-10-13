"""Generate private game sources and a bounded native execution oracle."""
from pathlib import Path
import shutil
import subprocess

RUNTIME = ('cpu', 'm2_board', 'm2_tgp_board', 'tgp', 'geo', 'video', 'raster', 'lockstep', 'comm_board', 'enhance')


def prepare_game(root, out, host_name='xbox360-host-tools', frames=120, raster_diagnostics=False):
    repo = Path(__file__).resolve().parents[2]
    host = root / host_name
    subprocess.run(['cmake', '-S', str(repo), '-B', str(host), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release', '-DM2_ROMSET=daytona93'], check=True)
    subprocess.run(['cmake', '--build', str(host), '--target', 'm2import', 'm2recomp',
                    'm2tgprecomp', '--parallel', '4'], check=True)
    cache = host / 'rom_cache/daytona93'
    rom = next((repo / 'roms' / ('daytona93.' + ext) for ext in ('zip', '7z')
                if (repo / 'roms' / ('daytona93.' + ext)).is_file()), None)
    if rom is None:
        raise RuntimeError('Provide your verified daytona93.zip or .7z in roms/')
    subprocess.run([str(host / 'm2import'), str(rom), str(cache)], check=True)
    gen, tgp = host / 'gen/daytona93', host / 'gen/daytona93_tgp'
    gen.mkdir(parents=True, exist_ok=True)
    tgp.mkdir(parents=True, exist_ok=True)
    # Fixed importer/recompiler and fresh source list; no other build's output.
    for old in gen.glob('*.cpp'):
        old.unlink()
    subprocess.run([str(host / 'm2recomp'), str(cache / 'program.bin'), str(gen),
                    '--seeds', str(repo / 'seeds/daytona93.txt'),
                    '--hooks', str(repo / 'seeds/daytona93_hooks.txt')], check=True)
    subprocess.run([str(host / 'm2tgprecomp'), str(cache / 'tgp_program.bin'),
                    str(tgp / 'tgp_gen.cpp')], check=True)
    assets = ('program', 'main_data', 'copro_tables', 'copro_data', 'polygons', 'textures')
    deploy = out / 'roms'
    deploy.mkdir(exist_ok=True)
    lines = ['struct GameAsset { const char *name; unsigned size; unsigned long long hash; };',
             'static constexpr GameAsset game_assets[] = {']
    for name in assets:
        data = (cache / (name + '.bin')).read_bytes()
        h = 14695981039346656037
        for b in data:
            h = ((h ^ b) * 1099511628211) & 0xffffffffffffffff
        lines.append(f'{{"{name}.bin", {len(data)}, 0x{h:016x}ull}},')
        shutil.copyfile(cache / (name + '.bin'), deploy / (name + '.bin'))
    (out / 'game_assets.h').write_text('\n'.join(lines) + '\n};\n')
    sources = [repo / 'src/runtime' / (n + '.cpp') for n in RUNTIME]
    sources += sorted(gen.glob('*.cpp')) + [tgp / 'tgp_gen.cpp']
    # Use the same isolated feature boundaries for host and target.
    oracle = out / 'host-game-oracle'
    subprocess.run(['clang++', '-std=c++20', '-O2', '-fno-fast-math', '-ffp-contract=off',
                    *(['-DM2_RASTER_DIAGNOSTICS'] if raster_diagnostics else []),
                    '-DM2_XBOX360_HEADLESS', '-DXBOX_GAME_HOST_TEST', f'-DGAME_PROBE_FRAMES={frames}', '-I', str(repo / 'src'),
                    '-I', str(out), str(repo / 'platform/xbox360/game_probe.cpp'),
                    *map(str, sources), '-o', str(oracle)], check=True)
    with (out / 'host-game.log').open('w') as log:
        subprocess.run([str(oracle), str(deploy), str(out / 'game_expected.h')],
                       stdout=log, stderr=log, check=True)
    return sources
