#!/usr/bin/env python3
"""Build ROM-free OpenXeChain probes, never the game."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_image import prepare, validate_xex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--out", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--display-probe", action="store_true", help="Read display information without changing GPU state")
    mode.add_argument("--framebuffer-probe", action="store_true", help="Test physical framebuffer allocation without GPU submission")
    mode.add_argument("--check-cxx", action="store_true",
                      help="Instead test C++20 link support; not a gameplay build")
    mode.add_argument("--input-probe", action="store_true", help="Build separate controller log probe")
    mode.add_argument("--cxx-probe", action="store_true", help="Build restricted C++ hardware probe, not full ABI")
    mode.add_argument("--thread-probe", action="store_true", help="Build native thread/atomic/kernel TLS probe")
    mode.add_argument("--tls-probe", action="store_true", help="Build compiler thread_local adapter probe")
    parser.add_argument("--libcxx", type=Path, default=Path("build/xbox360-libcxx"))
    args = parser.parse_args()
    default_out = Path("build/xbox360-current" if args.display_probe or args.framebuffer_probe else
                       "build/xbox360-tls" if args.tls_probe else
                       "build/xbox360-thread" if args.thread_probe else
                       "build/xbox360-cxx" if args.cxx_probe else
                       "build/xbox360-input" if args.input_probe else "build/xbox360-smoke")
    sdk, out = args.sdk.resolve(), (args.out or default_out).resolve()
    compiler, packer = sdk / "bin/clang", sdk / "bin/synthxex"
    if args.check_cxx or args.cxx_probe or args.tls_probe:
        compiler = sdk / "bin/clang++"
    for tool in ((compiler,) if args.check_cxx else (compiler, packer)):
        if not tool.is_file():
            parser.error(f"Missing {tool}; finish the OpenXeChain toolchain build first")
    libcxx = args.libcxx.resolve()
    if args.cxx_probe:
        for rel in ("include/c++/v1/__config_site", "lib/libc++.a"):
            if not (libcxx / rel).is_file():
                parser.error("Run build_libcxx.py first; restricted libc++ is missing")
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="", LIBRARY_PATH="")
    if args.check_cxx:
        subprocess.run([str(compiler), "--target=ppc32-xbox360", "-std=c++20",
                        str(Path(__file__).resolve().parent / "probe_cpp.cpp"),
                        "-o", str(out / "cpp-probe.exe")], env=env, check=True)
        print("C++ link probe passed; exceptions/TLS still require hardware execution")
        return
    source = Path(__file__).resolve().parent / ("input_probe.c" if args.input_probe else "smoke.c")
    if args.thread_probe:
        source = source.with_name("thread_probe.c")
    if args.display_probe:
        source = source.with_name("display_probe.c")
    if args.framebuffer_probe:
        source = source.with_name("framebuffer_probe.c")
    pe, xex = out / "smoke.exe", out / "default.xex"
    extra = []
    if args.tls_probe:
        source = source.with_name("tls_probe.cpp")
        extra = ["-femulated-tls", "-fno-exceptions", "-fno-rtti",
                 str(source.with_name("tls_runtime.cpp"))]
    if args.cxx_probe:
        source = Path(__file__).resolve().parent / "cxx_probe.cpp"
        extra = ["-fno-exceptions", "-fno-rtti", "-nostdinc++",
                 "-I", str(libcxx / "include/c++/v1"),
                 str(source.with_name("probe_alloc.cpp")), str(libcxx / "lib/libc++.a"),
                 "-Wl,/defaultlib:libm.a"]
    subprocess.run([str(compiler), "--target=ppc32-xbox360", "-std=c++20" if args.cxx_probe or args.tls_probe else "-std=c11", "-O2",
                    "-Wall", "-Wextra", "-fno-fast-math", "-ffp-contract=off",
                    str(source), *extra, "-o", str(pe)], env=env, check=True)
    packed_pe = out / "package.exe"
    prepare(pe, packed_pe)
    subprocess.run([str(packer), "-i", str(packed_pe), "-o", str(xex), "-t", "title"],
                   env=env, check=True)
    if xex.read_bytes()[:4] != b"XEX2":
        raise SystemExit("Packager output lacks XEX2 signature")
    validate_xex(packed_pe, xex)
    print(f"Built {xex}; full mapped image checked, console launch NOT verified")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
