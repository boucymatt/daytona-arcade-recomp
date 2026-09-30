#!/usr/bin/env python3
"""Prepare a private PSP Memory Stick folder using the verified host importer.

Never distribute the resulting folder: it contains the user's game-derived
code and ROM regions. Large regions stay on disk and are paged by the PSP.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", type=Path, required=True, help="your daytona93 zip or 7z")
    ap.add_argument("--host-build-dir", type=Path, default=ROOT / "build")
    ap.add_argument("--psp-build-dir", type=Path, default=ROOT / "build/psp")
    ap.add_argument("--output", type=Path, default=ROOT / "build/psp-install/PSP/GAME/DAYTONA")
    args = ap.parse_args(argv)
    rom = args.rom.expanduser().resolve()
    importer = args.host_build_dir.resolve() / "m2import"
    eboot = args.psp_build_dir.resolve() / "EBOOT.PBP"
    output = args.output.expanduser().resolve()
    if not rom.is_file():
        ap.error(f"missing ROM archive: {rom}")
    if not os.access(importer, os.X_OK):
        ap.error(f"missing host importer: {importer}; build the host project first")
    if not eboot.is_file():
        ap.error(f"missing PSP game: {eboot}; run scripts/build_psp.py first")
    if output.exists():
        ap.error("output already exists; choose a new folder to preserve its data")
    if output == ROOT or output in ROOT.parents:
        ap.error("output cannot be the source directory or one of its parents")
    if ROOT in output.parents:
        ignored = subprocess.run(["git", "check-ignore", "-q", str(output)], cwd=ROOT)
        if ignored.returncode:
            ap.error("game data must be under a git-ignored directory such as build/")
    with eboot.open("rb") as file:
        if file.read(4) != b"\x00PBP":
            ap.error("EBOOT.PBP has an invalid package header")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".daytona-psp-", dir=output.parent) as temporary:
        stage = Path(temporary) / "DAYTONA"
        stage.mkdir()
        subprocess.run([str(importer), str(rom), str(stage / "roms")], cwd=ROOT, check=True)
        shutil.copy2(eboot, stage / "EBOOT.PBP")
        shutil.copy2(ROOT / "platform/psp/README.md", stage / "README.md")
        licenses = stage / "licenses"
        licenses.mkdir()
        shutil.copy2(ROOT / "THIRD_PARTY.md", licenses / "THIRD_PARTY.md")
        shutil.copy2(ROOT / "platform/psp/THIRD_PARTY.md", licenses / "PSP.md")
        (stage / "INSTALL.txt").write_text(
            "Private build using your verified Daytona93 ROMs. Do not redistribute.\n"
            "Copy DAYTONA to PSP/GAME/DAYTONA on your Memory Stick.\n"
            "Requires a PSP configured to run homebrew; this package changes no firmware.\n"
            "PSP-1000: no expanded-RAM or ARK force-memory option is required.\n"
            "ROM pages are cached from roms/; this is not extra physical RAM.\n"
            "See the repository's platform/psp/README.md for controls and limitations.\n",
            encoding="utf-8")
        # The destination was absent; refuse to replace any newly created data.
        if output.exists():
            raise RuntimeError("output appeared during preparation; refusing to replace it")
        stage.rename(output)
    print(f"Private install folder: {output}\nCopy it to PSP/GAME/DAYTONA on the Memory Stick.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"prepare_psp: {error}", file=sys.stderr)
        sys.exit(1)
