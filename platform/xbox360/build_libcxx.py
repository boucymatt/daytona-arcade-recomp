#!/usr/bin/env python3
"""Build an isolated single-threaded, no-exceptions libc++ probe library.

Does not install into the SDK or enable this configuration for the game.
Uses only the already-pinned OpenXeChain LLVM source; never host C++ headers.
"""
import argparse
import os
from pathlib import Path
import subprocess

LLVM_PIN = "890b83f6c8259a8899e182a5f7d9cf39c64131cc"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="OpenXeChain buildscript checkout")
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("build/xbox360-libcxx"))
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    source, sdk, out = args.source.resolve(), args.sdk.resolve(), args.out.resolve()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    revision = subprocess.check_output(["git", "-C", str(source / "llvm"), "rev-parse", "HEAD"], text=True).strip()
    if revision != LLVM_PIN:
        parser.error("Unsupported LLVM revision; use the pinned OpenXeChain submodule")
    for tool in ("clang", "clang++", "llvm-ar", "llvm-ranlib"):
        if not (sdk / "bin" / tool).is_file():
            parser.error(f"Missing SDK tool: {tool}")
    options = {
        "CMAKE_SYSTEM_NAME": "Generic", "CMAKE_TRY_COMPILE_TARGET_TYPE": "STATIC_LIBRARY",
        "CMAKE_C_COMPILER": sdk / "bin/clang", "CMAKE_CXX_COMPILER": sdk / "bin/clang++",
        "CMAKE_AR": sdk / "bin/llvm-ar", "CMAKE_RANLIB": sdk / "bin/llvm-ranlib",
        "CMAKE_C_COMPILER_TARGET": "ppc32-xbox360", "CMAKE_CXX_COMPILER_TARGET": "ppc32-xbox360",
        "CMAKE_BUILD_TYPE": "Release", "LLVM_ENABLE_RUNTIMES": "libcxx",
        "LIBCXX_ENABLE_SHARED": "OFF", "LIBCXX_ENABLE_STATIC": "ON",
        # Generic turns even the excluded shared target into a static library:
        # separate its name to avoid duplicate Ninja output rules.
        "LIBCXX_SHARED_OUTPUT_NAME": "cxx-unused-shared",
        "LIBCXX_ENABLE_EXCEPTIONS": "OFF", "LIBCXX_ENABLE_RTTI": "OFF",
        "LIBCXX_ENABLE_THREADS": "OFF", "LIBCXX_ENABLE_MONOTONIC_CLOCK": "OFF",
        "LIBCXX_ENABLE_FILESYSTEM": "OFF", "LIBCXX_ENABLE_LOCALIZATION": "OFF",
        "LIBCXX_ENABLE_RANDOM_DEVICE": "OFF", "LIBCXX_CXX_ABI": "none",
        "LIBCXX_ENABLE_NEW_DELETE_DEFINITIONS": "OFF",
        "LIBCXX_INCLUDE_TESTS": "OFF", "LIBCXX_INCLUDE_BENCHMARKS": "OFF",
        # This fork emits undefined function symbols with function sections.
        # Keep the workaround local to this experimental library build.
        "LIBCXX_ADDITIONAL_COMPILE_FLAGS": "-fno-function-sections;-fno-data-sections;-fno-fast-math;-ffp-contract=off",
    }
    env = dict(os.environ, C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="", LIBRARY_PATH="")
    subprocess.run(["cmake", "-S", str(source / "llvm/runtimes"), "-B", str(out), "-G", "Ninja",
                    *[f"-D{key}={value}" for key, value in options.items()]], env=env, check=True)
    subprocess.run(["cmake", "--build", str(out), "--target", "cxx_static", "--parallel", str(args.jobs)],
                   env=env, check=True)
    for rel in ("include/c++/v1/__config_site", "lib/libc++.a"):
        if not (out / rel).is_file():
            raise SystemExit(f"Missing output: {out / rel}")
    print("Restricted libc++ built; NOT a game-ready C++ ABI/runtime")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
