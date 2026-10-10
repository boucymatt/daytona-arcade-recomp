#!/usr/bin/env python3
"""Complete pinned OpenXeChain after its compiler-rt host-LLVM discovery failure.

Requires the upstream script to have installed Clang, xecorelib and newlib.
Never rebuilds LLVM or deletes its build tree. Generated SDK configs are extended
idempotently with the same compiler-rt linkage as the upstream script.
"""
import argparse
import os
from pathlib import Path
import subprocess

PIN = "eed1fa65bf9577fd31625764b320a90182ea9ade"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("build/xbox360-sdk-completion"))
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    source, sdk, out = args.source.resolve(), args.sdk.resolve(), args.out.resolve()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if revision != PIN:
        parser.error("This completion step is only audited for the pinned buildscript revision")
    states = subprocess.check_output(["git", "-C", str(source), "submodule", "status"], text=True)
    if len(states.splitlines()) != 4 or any(not line.startswith(" ") for line in states.splitlines()):
        parser.error("Initialize all four submodules at their pinned revisions first")
    for rel in ("bin/clang", "bin/clang++", "bin/clang.cfg", "bin/clang++.cfg",
                "lib/xecorelib.a", "ppc-xbox360/lib/libc.a"):
        if not (sdk / rel).is_file():
            parser.error(f"Missing {sdk / rel}; upstream compiler/newlib stages must succeed first")
    env = dict(os.environ, C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="", LIBRARY_PATH="")

    def run(*command):
        subprocess.run([str(value) for value in command], env=env, check=True)

    rt = out / "compiler-rt"
    run("cmake", "-S", source / "llvm/compiler-rt", "-B", rt, "-G", "Ninja",
        f"-DCMAKE_INSTALL_PREFIX={sdk}", "-DCMAKE_SYSTEM_NAME=Generic", "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_C_COMPILER={sdk}/bin/clang", f"-DCMAKE_CXX_COMPILER={sdk}/bin/clang++",
        f"-DCMAKE_AR={sdk}/bin/llvm-ar", f"-DCMAKE_RANLIB={sdk}/bin/llvm-ranlib",
        f"-DCMAKE_LINKER={sdk}/bin/lld-link", f"-DCMAKE_SYSROOT={sdk}",
        "-DCMAKE_C_COMPILER_TARGET=ppc32-xbox360", "-DCMAKE_CXX_COMPILER_TARGET=ppc32-xbox360",
        "-DCMAKE_C_COMPILER_WORKS=TRUE", "-DCMAKE_CXX_COMPILER_WORKS=TRUE",
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY", "-DLLVM_RUNTIMES_BUILD=ON",
        f"-DCMAKE_MODULE_PATH={source}/llvm/llvm/cmake/modules",
        "-DCOMPILER_RT_BUILD_BUILTINS=ON", "-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON",
        "-DCOMPILER_RT_STANDALONE_BUILD=ON", "-DCOMPILER_RT_BAREMETAL_BUILD=ON",
        "-DCOMPILER_RT_BUILTINS_ENABLE_PIC=OFF", "-DCOMPILER_RT_BUILD_SANITIZERS=OFF",
        "-DCOMPILER_RT_BUILD_XRAY=OFF", "-DCOMPILER_RT_BUILD_LIBFUZZER=OFF",
        "-DCOMPILER_RT_BUILD_PROFILE=OFF", "-DCOMPILER_RT_BUILD_SCUDO_STANDALONE=OFF")
    run("cmake", "--build", rt, "--parallel", args.jobs)
    run("cmake", "--install", rt)
    if not (sdk / "lib/generic/libclang_rt.builtins-powerpc.a").is_file():
        raise SystemExit("compiler-rt install did not produce the PowerPC builtins archive")
    for name in ("clang.cfg", "clang++.cfg"):
        config = sdk / "bin" / name
        original = config.read_text()
        updated = original
        for line in ("-Wl,/libpath:<CFGDIR>/../lib/generic",
                     "-Wl,/defaultlib:libclang_rt.builtins-powerpc.a"):
            if line not in updated.splitlines():
                updated = updated.rstrip() + "\n" + line + "\n"
        if updated != original:
            config.write_text(updated)
    packer = out / "synthxex"
    run("cmake", "-S", source / "synthxex", "-B", packer, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=/usr/bin/cc",
        "-DCMAKE_CXX_COMPILER=/usr/bin/c++", f"-DCMAKE_INSTALL_PREFIX={sdk}")
    run("cmake", "--build", packer, "--parallel", args.jobs)
    run("cmake", "--install", packer)
    print("OpenXeChain C toolchain completion succeeded; C++ runtime is NOT supplied")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
