"""Host-side PSP build validation; no toolchain or game data required."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("build_psp", Path(__file__).resolve().parents[1] / "scripts/build_psp.py")
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class BuildPspTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "project"
        self.root.mkdir()
        self.sdk = Path(self.temp.name) / "sdk with spaces"
        (self.sdk / "psp/share").mkdir(parents=True)
        (self.sdk / "psp/share/pspdev.cmake").touch()
        (self.sdk / "bin").mkdir()
        for tool in BUILD.SDK_TOOLS:
            path = self.sdk / "bin" / tool
            path.touch()
            path.chmod(0o700)
        patcher = patch.object(BUILD, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.output = io.StringIO()

    def call(self, *args):
        with contextlib.redirect_stdout(self.output), contextlib.redirect_stderr(self.output):
            return BUILD.main(["--pspdev", str(self.sdk), *args])

    def make_generated(self):
        for name in BUILD.REQUIRED:
            path = self.root / "build/gen" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()  # Presence marker, not generated game code.

    def test_missing_sdk(self):
        with self.assertRaises(SystemExit):
            self.call("--pspdev", str(self.root / "missing"))
        self.assertIn("missing PSPDEV toolchain", self.output.getvalue())

    def test_missing_compiler(self):
        (self.sdk / "bin/psp-g++").unlink()
        with self.assertRaises(SystemExit):
            self.call("--compile-check")
        self.assertIn("missing executable PSPDEV tools", self.output.getvalue())

    def test_compile_check_does_not_need_packager(self):
        (self.sdk / "bin/pack-pbp").unlink()
        with patch.object(BUILD.subprocess, "run"):
            self.assertEqual(self.call("--compile-check"), 0)

    def test_real_build_requires_packager(self):
        (self.sdk / "bin/pack-pbp").unlink()
        with self.assertRaises(SystemExit):
            self.call()

    def test_missing_generated_code_never_invokes_build(self):
        with patch.object(BUILD.subprocess, "run") as run:
            with self.assertRaises(SystemExit):
                self.call()
            run.assert_not_called()
        self.assertIn("host-generated code", self.output.getvalue())

    def test_reject_unsafe_build_directories(self):
        for path in (".", "..", "build", "build/gen", "build/gen/psp"):
            with self.subTest(path=path), self.assertRaises(SystemExit):
                self.call("--compile-check", "--build-dir", path)

    def test_reject_nonpositive_jobs(self):
        for jobs in ("0", "-2"):
            with self.subTest(jobs=jobs), self.assertRaises(SystemExit):
                self.call("--compile-check", "--jobs", jobs)

    def test_compile_check_only_runs_cmake(self):
        with patch.object(BUILD.subprocess, "run") as run:
            self.assertEqual(self.call("--compile-check"), 0)
        self.assertEqual(run.call_count, 2)
        configure, compile_ = run.call_args_list
        self.assertEqual(configure.args[0][0], "cmake")
        self.assertIn("-DDAYTONA_PSP_COMPILE_CHECK=ON", configure.args[0])
        self.assertIn("-DCMAKE_BUILD_TYPE=MinSizeRel", configure.args[0])
        self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={self.sdk}/psp/share/pspdev.cmake", configure.args[0])
        self.assertEqual(compile_.args[0][0:2], ["cmake", "--build"])
        self.assertEqual(configure.kwargs["env"]["PSPDEV"], str(self.sdk))
        self.assertTrue(configure.kwargs["env"]["PATH"].startswith(str(self.sdk / "bin") + os.pathsep))
        self.assertIn("No linked game or EBOOT.PBP", self.output.getvalue())

    def test_success_requires_package(self):
        self.make_generated()
        with patch.object(BUILD.subprocess, "run"), self.assertRaises(RuntimeError):
            self.call()

    def test_package_reports_unverified_hardware(self):
        self.make_generated()
        (self.root / "build/psp").mkdir()
        (self.root / "build/psp/EBOOT.PBP").touch()
        with patch.object(BUILD.subprocess, "run") as run:
            self.assertEqual(self.call("--jobs", "3"), 0)
        self.assertIn("-DDAYTONA_PSP_COMPILE_CHECK=OFF", run.call_args_list[0].args[0])
        self.assertIn("does not verify hardware", self.output.getvalue())
        self.assertEqual(run.call_args_list[1].args[0][-2:], ["--parallel", "3"])

    def test_custom_host_directory(self):
        with patch.object(BUILD.subprocess, "run") as run:
            self.call("--compile-check", "--host-build-dir", "host build", "--build-dir", "cross build")
        self.assertIn(f"-DDAYTONA_GEN_ROOT={self.root}/host build/gen", run.call_args_list[0].args[0])

    def test_subprocess_failure_propagates(self):
        with patch.object(BUILD.subprocess, "run", side_effect=BUILD.subprocess.CalledProcessError(1, "cmake")):
            with self.assertRaises(BUILD.subprocess.CalledProcessError):
                self.call("--compile-check")


if __name__ == "__main__":
    unittest.main()
