"""Isolated native oracle and big-endian Xbox SoftFloat libraries."""
from pathlib import Path
import shlex
import subprocess


def prepare_fp(sdk, root, compiler, runtime, out):
    source = Path(__file__).resolve().parent / 'softfloat'
    softfloat = source.parents[2] / 'extern/softfloat'
    revision = subprocess.check_output(['git', '-C', str(softfloat), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != 'a0c6494cdc11865811dec815d5c0049fba9d82a8':
        raise RuntimeError('Unexpected SoftFloat revision')
    host, target = root / 'xbox360-fp-host', root / 'xbox360-fp-lib'
    subprocess.run(['cmake', '-S', str(source), '-B', str(host), '-G', 'Ninja',
                    '-DCMAKE_BUILD_TYPE=Release'], check=True)
    subprocess.run(['cmake', '--build', str(host), '--parallel', '6'], check=True)
    expected = subprocess.check_output([str(host / 'fp_oracle')], text=True)
    (out / 'fp_expected.h').write_text(expected)
    flags = shlex.join(['--config=' + str(sdk / 'bin/clang++.cfg')])
    flags += ' -femulated-tls -fdwarf-exceptions -funwind-tables -fno-function-sections -fno-data-sections'
    options = dict(CMAKE_SYSTEM_NAME='Generic', CMAKE_TRY_COMPILE_TARGET_TYPE='STATIC_LIBRARY',
                   CMAKE_C_COMPILER=compiler, CMAKE_CXX_COMPILER=compiler,
                   CMAKE_C_COMPILER_TARGET='ppc32-xbox360', CMAKE_CXX_COMPILER_TARGET='ppc32-xbox360',
                   CMAKE_AR=sdk / 'bin/llvm-ar', CMAKE_RANLIB=sdk / 'bin/llvm-ranlib',
                   CMAKE_C_FLAGS=flags, CMAKE_CXX_FLAGS=flags + ' -nostdinc++ -I' +
                   shlex.quote(str(runtime / 'include/c++/v1')),
                   CMAKE_BUILD_TYPE='Release', XBOX_TARGET='ON')
    subprocess.run(['cmake', '-S', str(source), '-B', str(target), '-G', 'Ninja',
                    *[f'-D{k}={v}' for k, v in options.items()]], check=True)
    # Compiler binaries are not tracked as Ninja object prerequisites.
    subprocess.run(['cmake', '--build', str(target), '--target', 'clean'], check=True)
    subprocess.run(['cmake', '--build', str(target), '--parallel', '6'], check=True)
    return [target / 'libprobe_fp.a', target / 'libprobe_softfloat.a']
