#!/usr/bin/env python3
"""Build experimental exception dependencies without replacing the passing SDK.

Stages are explicit. Neither stage packages an XEX or claims exception support.
"""
import argparse
import hashlib
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--stage', choices=('compiler', 'runtime', 'linker'), required=True)
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--build-root', type=Path, default=Path('build'))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    source, sdk, root = args.source.resolve(), args.sdk.resolve(), args.build_root.resolve()
    subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_exceptions.py')),
                    '--source', str(source)], check=True)
    compiler_build = root / 'xbox360-llvm-eh'
    if args.stage in ('compiler', 'linker'):
        tree, out, target = source / 'llvm/llvm', compiler_build, 'lld' if args.stage == 'linker' else 'clang'
        options = dict(CMAKE_C_COMPILER='clang', CMAKE_CXX_COMPILER='clang++',
                       CMAKE_BUILD_TYPE='Release', LLVM_TARGETS_TO_BUILD='PowerPC',
                       LLVM_ENABLE_PROJECTS='clang;lld', LLVM_INCLUDE_BENCHMARKS='OFF',
                       LLVM_INCLUDE_TESTS='OFF', LLVM_INCLUDE_EXAMPLES='OFF',
                       LLVM_ENABLE_BINDINGS='OFF', LLVM_ENABLE_ZSTD='OFF')
    else:
        clang = compiler_build / 'bin/clang'
        if not clang.is_file():
            parser.error('Build the isolated compiler stage first')
        tree, out, target = source / 'llvm/runtimes', root / 'xbox360-exception-runtime', 'cxxabi_static'
        config = shlex.join(['--config=' + str(sdk / 'bin/clang++.cfg')])
        flags = config + ' -fno-function-sections -fno-data-sections -fno-fast-math -ffp-contract=off'
        options = dict(CMAKE_BUILD_TYPE='Release', CMAKE_SYSTEM_NAME='Generic',
                       CMAKE_TRY_COMPILE_TARGET_TYPE='STATIC_LIBRARY',
                       CMAKE_C_COMPILER=clang, CMAKE_CXX_COMPILER=clang, CMAKE_ASM_COMPILER=clang,
                       CMAKE_C_COMPILER_TARGET='ppc32-xbox360', CMAKE_CXX_COMPILER_TARGET='ppc32-xbox360',
                       CMAKE_ASM_COMPILER_TARGET='ppc32-xbox360',
                       CMAKE_C_FLAGS=flags + ' -fdwarf-exceptions -funwind-tables', CMAKE_CXX_FLAGS=flags + ' -fdwarf-exceptions -funwind-tables',
                       CMAKE_ASM_FLAGS=config, CMAKE_AR=sdk / 'bin/llvm-ar',
                       CMAKE_RANLIB=sdk / 'bin/llvm-ranlib',
                       LLVM_ENABLE_RUNTIMES='libunwind;libcxxabi;libcxx',
                       LIBCXX_ENABLE_SHARED='OFF', LIBCXX_SHARED_OUTPUT_NAME='cxx-unused-shared',
                       LIBCXX_ENABLE_THREADS='OFF', LIBCXX_ENABLE_MONOTONIC_CLOCK='OFF',
                       LIBCXX_ENABLE_LOCALIZATION='OFF', LIBCXX_ENABLE_FILESYSTEM='OFF',
                       LIBCXX_ENABLE_RANDOM_DEVICE='OFF', LIBCXX_INCLUDE_TESTS='OFF',
                       LIBCXX_INCLUDE_BENCHMARKS='OFF', LIBCXXABI_ENABLE_SHARED='OFF',
                       LIBCXXABI_SHARED_OUTPUT_NAME='cxxabi-unused-shared',
                       LIBCXXABI_ENABLE_THREADS='OFF', LIBCXXABI_INCLUDE_TESTS='OFF',
                       LIBUNWIND_ENABLE_SHARED='OFF', LIBUNWIND_SHARED_OUTPUT_NAME='unwind-unused-shared',
                       LIBUNWIND_ENABLE_THREADS='OFF', LIBUNWIND_IS_BAREMETAL='ON',
                       LIBUNWIND_INCLUDE_TESTS='OFF', LIBUNWIND_HIDE_SYMBOLS='ON')
        for library in ('LIBCXX', 'LIBCXXABI', 'LIBUNWIND'):
            options[library + '_ADDITIONAL_COMPILE_FLAGS'] = '-fno-function-sections;-fno-data-sections'
        options['LIBUNWIND_ADDITIONAL_COMPILE_FLAGS'] += ';-fms-extensions'
    subprocess.run(['cmake', '-S', str(tree), '-B', str(out), '-G', 'Ninja',
                    *[f'-D{key}={value}' for key, value in options.items()]], check=True)
    if args.stage == 'runtime':
        # Ninja does not track a rebuilt compiler binary as an object dependency.
        subprocess.run(['cmake', '--build', str(out), '--target', 'clean'], check=True)
    subprocess.run(['cmake', '--build', str(out), '--target', target, '--parallel', str(args.jobs)], check=True)
    if args.stage == 'runtime':
        (out / 'compiler.sha256').write_text(hashlib.sha256(clang.read_bytes()).hexdigest())
    print(f'{args.stage} dependency build finished; exception linking/execution NOT verified')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
