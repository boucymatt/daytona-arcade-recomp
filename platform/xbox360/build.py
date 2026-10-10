#!/usr/bin/env python3
"""Build ROM-free OpenXeChain probes, never the game."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--out", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check-cxx", action="store_true",
                      help="Instead test C++20 link support; not a gameplay build")
    mode.add_argument("--input-probe", action="store_true", help="Build separate controller log probe")
    args = parser.parse_args()
    default_out = Path("build/xbox360-input" if args.input_probe else "build/xbox360-smoke")
    sdk, out = args.sdk.resolve(), (args.out or default_out).resolve()
    compiler, packer = sdk / "bin/clang", sdk / "bin/synthxex"
    if args.check_cxx:
        compiler = sdk / "bin/clang++"
    for tool in ((compiler,) if args.check_cxx else (compiler, packer)):
        if not tool.is_file():
            parser.error(f"Missing {tool}; finish the OpenXeChain toolchain build first")
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="", LIBRARY_PATH="")
    if args.check_cxx:
        subprocess.run([str(compiler), "--target=ppc32-xbox360", "-std=c++20",
                        str(Path(__file__).resolve().parent / "probe_cpp.cpp"),
                        "-o", str(out / "cpp-probe.exe")], env=env, check=True)
        print("C++ link probe passed; exceptions/TLS still require hardware execution")
        return
    source = Path(__file__).resolve().parent / ("input_probe.c" if args.input_probe else "smoke.c")
    pe, xex = out / "smoke.exe", out / "default.xex"
    subprocess.run([str(compiler), "--target=ppc32-xbox360", "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-fno-fast-math", "-ffp-contract=off",
                    str(source), "-o", str(pe)], env=env, check=True)
    subprocess.run([str(packer), "-i", str(pe), "-o", str(xex), "-t", "title"],
                   env=env, check=True)
    if xex.read_bytes()[:4] != b"XEX2":
        raise SystemExit("Packager output lacks XEX2 signature")
    print(f"Built {xex}; header signature checked, console launch NOT verified")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
