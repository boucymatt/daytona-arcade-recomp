"""Memory-test build contracts; no PSP toolchain or ROM data required."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/test_psp_memory.sh"


@unittest.skipUnless(os.name == "posix" and shutil.which("bash"),
                     "POSIX shell runner requires bash and executable scripts")
class PspMemoryScriptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.build = self.directory / "host build"
        self.build.mkdir()
        for name in ("daytona93", "daytona93_tgp", "daytona93_snd"):
            path = self.build / "CMakeFiles/m2run.dir/gen" / name
            path.mkdir(parents=True)
            (path / "placeholder.o").touch()
        self.compiler_log = self.directory / "compiler.json"
        self.runner_log = self.directory / "runner.json"
        self.compiler = self.directory / "fake compiler"
        self.compiler.write_text(
            "#!/usr/bin/env python3\n"
            "import json, os, pathlib, sys\n"
            "pathlib.Path(os.environ['COMPILER_LOG']).write_text(json.dumps(sys.argv[1:]))\n"
            "output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
            "output.write_text('#!/usr/bin/env python3\\nimport json, os, pathlib, sys\\n'"
            " + \"pathlib.Path(os.environ['RUNNER_LOG']).write_text(json.dumps(sys.argv))\\n\")\n"
            "output.chmod(0o700)\n"
        )
        self.compiler.chmod(0o700)

    def run_script(self, native=None):
        env = dict(os.environ, SKIP_BUILD="1", CXX=str(self.compiler),
                   COMPILER_LOG=str(self.compiler_log), RUNNER_LOG=str(self.runner_log))
        env.pop("PSP_NATIVE_VIDEO", None)
        if native is not None:
            env["PSP_NATIVE_VIDEO"] = native
        return subprocess.run(
            ["bash", str(SCRIPT), str(self.build), "1680", "ROM path", "attract", "1440", "240"],
            env=env, capture_output=True, text=True, check=False)

    def test_default_preserves_reference_build(self):
        result = self.run_script()
        self.assertEqual(result.returncode, 0, result.stderr)
        args = json.loads(self.compiler_log.read_text())
        self.assertIn("-DM2_LOW_MEMORY=1", args)
        self.assertNotIn("-DM2_PSP_NATIVE_VIDEO=1", args)
        self.assertEqual(args[-1], str(self.build / "test_psp_memory_rom"))
        self.assertEqual(json.loads(self.runner_log.read_text())[1:],
                         ["ROM path", "1680", "attract", "1440", "240"])

    def test_native_uses_separate_build(self):
        result = self.run_script("1")
        self.assertEqual(result.returncode, 0, result.stderr)
        args = json.loads(self.compiler_log.read_text())
        self.assertIn("-DM2_LOW_MEMORY=1", args)
        self.assertIn("-DM2_PSP_NATIVE_VIDEO=1", args)
        self.assertEqual(args[-1], str(self.build / "test_psp_native_memory_rom"))
        self.assertFalse((self.build / "test_psp_memory_rom").exists())

    def test_explicit_reference_is_supported(self):
        result = self.run_script("0")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("-DM2_PSP_NATIVE_VIDEO=1", json.loads(self.compiler_log.read_text()))

    def test_invalid_switch_fails_before_compiling(self):
        for value in ("2", "ON", "-1"):
            with self.subTest(value=value):
                result = self.run_script(value)
                self.assertEqual(result.returncode, 2)
                self.assertIn("PSP_NATIVE_VIDEO must be", result.stderr)
                self.assertFalse(self.compiler_log.exists())


if __name__ == "__main__":
    unittest.main()
