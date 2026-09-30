#!/usr/bin/env python3
"""ROM-free checks for private PSP installation staging and overwrite guards."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("prepare_psp", Path(__file__).resolve().parents[1] / "scripts/prepare_psp.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PreparePSPTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="daytona-psp-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.rom = self.root / "own.zip"
        self.rom.write_bytes(b"synthetic test input")
        self.host = self.root / "host"
        self.host.mkdir()
        (self.host / "m2import").write_bytes(b"test stub")
        (self.host / "m2import").chmod(0o700)
        self.build = self.root / "psp"
        self.build.mkdir()
        (self.build / "EBOOT.PBP").write_bytes(b"\x00PBPtest-only")
        self.output = self.root / "install/DAYTONA"

    def invoke(self, output=None):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return MODULE.main(["--rom", str(self.rom), "--host-build-dir", str(self.host),
                                "--psp-build-dir", str(self.build), "--output", str(output or self.output)])

    def fake_import(self, command, **kwargs):
        self.assertEqual(Path(command[0]), self.host / "m2import")
        self.assertEqual(Path(command[1]), self.rom)
        regions = Path(command[2])
        regions.mkdir()
        (regions / "synthetic.bin").write_bytes(b"not game data")

    def test_stages_package_and_verified_import(self):
        with patch.object(MODULE.subprocess, "run", side_effect=self.fake_import) as importer:
            self.assertEqual(self.invoke(), 0)
        self.assertEqual(importer.call_count, 1)
        self.assertEqual((self.output / "EBOOT.PBP").read_bytes(), b"\x00PBPtest-only")
        self.assertTrue((self.output / "roms/synthetic.bin").is_file())
        self.assertTrue((self.output / "licenses/PSP.md").is_file())
        self.assertIn("Do not redistribute", (self.output / "INSTALL.txt").read_text())
        self.assertFalse((self.output / "smoke.txt").exists())

    def test_existing_output_is_preserved(self):
        self.output.mkdir(parents=True)
        saved = self.output / "save.bin"
        saved.write_bytes(b"saved")
        with self.assertRaises(SystemExit):
            self.invoke()
        self.assertEqual(saved.read_bytes(), b"saved")

    def test_bad_package_header(self):
        (self.build / "EBOOT.PBP").write_bytes(b"wrong")
        with self.assertRaises(SystemExit):
            self.invoke()
        self.assertFalse(self.output.exists())

    def test_missing_rom(self):
        self.rom.unlink()
        with self.assertRaises(SystemExit):
            self.invoke()

    def test_import_failure_leaves_no_partial_install(self):
        with patch.object(MODULE.subprocess, "run", side_effect=RuntimeError("import failed")):
            with self.assertRaisesRegex(RuntimeError, "import failed"):
                self.invoke()
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.output.parent.iterdir()), [])

    def test_destination_created_during_import_is_preserved(self):
        def racing_import(command, **kwargs):
            self.fake_import(command, **kwargs)
            self.output.mkdir()
            (self.output / "save.bin").write_bytes(b"saved")
        with patch.object(MODULE.subprocess, "run", side_effect=racing_import):
            with self.assertRaisesRegex(RuntimeError, "appeared"):
                self.invoke()
        self.assertEqual((self.output / "save.bin").read_bytes(), b"saved")

    def test_unignored_source_destination_refused(self):
        output = MODULE.ROOT / "psp-private-install-test"
        with patch.object(MODULE.subprocess, "run") as command:
            command.return_value.returncode = 1
            with self.assertRaises(SystemExit):
                self.invoke(output)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
