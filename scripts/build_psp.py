#!/usr/bin/env python3
"""Cross-compile the PSP-1000 frontend using host-generated game code.

Uses the public PSPDEV/PSPSDK toolchain. Run setup/recompile with your own ROMs
on the host first. This command does not import or copy ROMs, run target tools,
change firmware, or request expanded RAM. --compile-check needs no game code
and produces no linked game or EBOOT.PBP.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = ("daytona93/gen_table.cpp", "daytona93_tgp/tgp_gen.cpp", "daytona93_snd/snd_gen.cpp")
SDK_TOOLS = ("psp-gcc", "psp-g++", "psp-fixup-imports", "psp-prxgen", "mksfoex", "pack-pbp")


def run(command, env):
    command = list(map(str, command))
    print("+ " + shlex.join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host-build-dir", type=Path, default=Path("build"))
    ap.add_argument("--build-dir", type=Path, default=Path("build/psp"))
    ap.add_argument("--pspdev", type=Path, default=os.environ.get("PSPDEV"))
    ap.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 6))
    ap.add_argument("--compile-check", action="store_true")
    args = ap.parse_args(argv)
    if args.jobs < 1:
        ap.error("--jobs must be positive")
    if args.pspdev is None:
        ap.error("set PSPDEV or pass --pspdev /path/to/pspdev")
    sdk = args.pspdev.expanduser().resolve()
    toolchain = sdk / "psp/share/pspdev.cmake"
    if not toolchain.is_file():
        ap.error(f"missing PSPDEV toolchain: {toolchain}")
    required_tools = SDK_TOOLS[:2] if args.compile_check else SDK_TOOLS
    missing_tools = [str(sdk / "bin" / tool) for tool in required_tools
                     if not os.access(sdk / "bin" / tool, os.X_OK)]
    if missing_tools:
        ap.error("missing executable PSPDEV tools:\n" + "\n".join(missing_tools))
    host = (ROOT / args.host_build_dir).resolve()
    build = (ROOT / args.build_dir).resolve()
    gen = host / "gen"
    if (build == host or build == ROOT or build in ROOT.parents or
            build in host.parents or build == gen or gen in build.parents):
        ap.error("use a separate cross-build directory, not the source/host build or generated sources")
    if not args.compile_check:
        missing = [str(gen / name) for name in REQUIRED if not (gen / name).is_file()]
        if missing:
            ap.error("missing host-generated code:\n" + "\n".join(missing) +
                     "\nRun python3 scripts/recompile.py with the host compiler first.")
    env = os.environ.copy()
    env["PSPDEV"] = str(sdk)
    env["PATH"] = str(sdk / "bin") + os.pathsep + env.get("PATH", "")
    run(["cmake", "-S", ROOT / "platform/psp", "-B", build,
         f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", "-DCMAKE_BUILD_TYPE=MinSizeRel",
         f"-DDAYTONA_GEN_ROOT={gen}",
         f"-DDAYTONA_PSP_COMPILE_CHECK={'ON' if args.compile_check else 'OFF'}"], env)
    run(["cmake", "--build", build, "--parallel", args.jobs], env)
    if args.compile_check:
        print("Compile check complete. No linked game or EBOOT.PBP was built.")
    else:
        package = build / "EBOOT.PBP"
        if not package.is_file():
            raise RuntimeError(f"build completed without the expected package: {package}")
        print(f"EBOOT.PBP: {package}\nTarget: PSP-1000 memory budget (no expanded RAM).\n"
              "Prepare your verified ROM data separately with scripts/prepare_psp.py.\n"
              "Cross-compilation does not verify hardware gameplay or speed.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"build_psp: {error}", file=sys.stderr)
        sys.exit(1)
