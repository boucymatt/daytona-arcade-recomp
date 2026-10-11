"""Host-only build orchestration tests; no SDK or console execution."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("xbox_build", Path(__file__).with_name("build.py"))
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class BuildTest(unittest.TestCase):
    def test_display_probe_isolated(self):
        self.check_isolated_probe("--display-probe", "display_probe.c")

    def test_framebuffer_probe_isolated(self):
        self.check_isolated_probe("--framebuffer-probe", "framebuffer_probe.c")

    def check_isolated_probe(self, option, source):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            for tool in ("clang", "synthxex"):
                (root / "sdk/bin" / tool).touch()
            calls = []
            def run(args, **kwargs):
                calls.append(args)
                if args[0].endswith("synthxex"):
                    (root / "out/default.xex").write_bytes(b"XEX2")
            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), option]), \
                    patch.object(builder.subprocess, "run", side_effect=run):
                builder.main()
            self.assertTrue(any(arg.endswith(source) for arg in calls[0]))
            self.assertIn("-std=c11", calls[0])
            self.assertFalse(any("game_probe" in arg or "libc++" in arg for arg in calls[0]))

    def setUp(self):
        # PE conversion/extent verification has real binary fixture tests in
        # test_package_image.py; these tests isolate command orchestration.
        for name in ('prepare', 'validate_xex'):
            mocked = patch.object(builder, name)
            mocked.start()
            self.addCleanup(mocked.stop)

    def test_compiler_tls_uses_adapter_and_no_libcxx(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            for tool in ("clang++", "synthxex"):
                (root / "sdk/bin" / tool).touch()
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                if args[0].endswith("synthxex"):
                    (root / "out/default.xex").write_bytes(b"XEX2")

            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), "--tls-probe"]), \
                    patch.object(builder.subprocess, "run", side_effect=run):
                builder.main()
            self.assertEqual(len(calls), 2)
            for flag in ("-std=c++20", "-femulated-tls", "-fno-exceptions"):
                self.assertIn(flag, calls[0])
            self.assertTrue(any(arg.endswith("tls_runtime.cpp") for arg in calls[0]))
            self.assertTrue(any(arg.endswith("tls_probe.cpp") for arg in calls[0]))
            self.assertFalse(any("libc++" in arg for arg in calls[0]))

    def test_thread_probe_isolated_from_cxx_library(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            for tool in ("clang", "synthxex"):
                (root / "sdk/bin" / tool).touch()
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                if args[0].endswith("synthxex"):
                    (root / "out/default.xex").write_bytes(b"XEX2")

            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), "--thread-probe"]), \
                    patch.object(builder.subprocess, "run", side_effect=run):
                builder.main()
            self.assertEqual(len(calls), 2)
            self.assertIn("-std=c11", calls[0])
            self.assertTrue(any(arg.endswith("thread_probe.c") for arg in calls[0]))
            self.assertFalse(any("libc++" in arg for arg in calls[0]))

    def test_restricted_cxx_build_commands(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sdk, lib, out = root / "sdk", root / "lib with spaces", root / "out"
            (sdk / "bin").mkdir(parents=True)
            for tool in ("clang++", "synthxex"):
                (sdk / "bin" / tool).touch()
            for rel in ("include/c++/v1/__config_site", "lib/libc++.a"):
                file = lib / rel
                file.parent.mkdir(parents=True, exist_ok=True)
                file.touch()
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                if args[0].endswith("synthxex"):
                    (out / "default.xex").write_bytes(b"XEX2")

            with patch("sys.argv", ["build.py", "--sdk", str(sdk), "--out", str(out),
                                    "--cxx-probe", "--libcxx", str(lib)]), \
                    patch.object(builder.subprocess, "run", side_effect=run):
                builder.main()
            self.assertEqual(len(calls), 2)
            for flag in ("-std=c++20", "-fno-exceptions", "-fno-rtti", "-nostdinc++", "-ffp-contract=off"):
                self.assertIn(flag, calls[0])
            self.assertIn(str(lib / "lib/libc++.a"), calls[0])
            self.assertTrue(any(arg.endswith("probe_alloc.cpp") for arg in calls[0]))

    def test_restricted_cxx_requires_library_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            for tool in ("clang++", "synthxex"):
                (root / "sdk/bin" / tool).touch()
            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), "--cxx-probe",
                                    "--libcxx", str(root / "missing")]):
                with self.assertRaises(SystemExit):
                    builder.main()
            self.assertFalse((root / "out").exists())

    def test_cxx_gate_does_not_package(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            (root / "sdk/bin/clang++").touch()
            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), "--check-cxx"]), \
                    patch.object(builder.subprocess, "run") as run:
                builder.main()
            self.assertEqual(run.call_count, 1)
            self.assertIn("-std=c++20", run.call_args.args[0])
            self.assertTrue(any(arg.endswith("probe_cpp.cpp") for arg in run.call_args.args[0]))

    def test_compiler_failure_stops_packaging(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "sdk/bin").mkdir(parents=True)
            for tool in ("clang", "synthxex"):
                (root / "sdk/bin" / tool).touch()
            with patch("sys.argv", ["build.py", "--sdk", str(root / "sdk"),
                                    "--out", str(root / "out"), "--input-probe"]), \
                    patch.object(builder.subprocess, "run", side_effect=
                                 builder.subprocess.CalledProcessError(1, "clang")) as run:
                with self.assertRaises(builder.subprocess.CalledProcessError):
                    builder.main()
            self.assertEqual(run.call_count, 1)
            self.assertTrue(any(arg.endswith("input_probe.c") for arg in run.call_args.args[0]))

    def test_missing_sdk_fails_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with patch("sys.argv", ["build.py", "--sdk", str(root / "missing"),
                                    "--out", str(root / "out")]):
                with self.assertRaises(SystemExit) as error:
                    builder.main()
            self.assertEqual(error.exception.code, 2)
            self.assertFalse((root / "out").exists())

    def test_build_commands_and_signature(self):
        for valid in (True, False):
            with self.subTest(valid=valid), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                sdk = root / "sdk with spaces"
                (sdk / "bin").mkdir(parents=True)
                for tool in ("clang", "synthxex"):
                    (sdk / "bin" / tool).touch()
                out = root / "output with spaces"
                calls = []

                def run(args, **kwargs):
                    calls.append(args)
                    self.assertTrue(kwargs["check"])
                    self.assertEqual(kwargs["env"]["CPLUS_INCLUDE_PATH"], "")
                    if args[0].endswith("synthxex"):
                        (out / "default.xex").write_bytes(b"XEX2" if valid else b"BAD!")

                with patch("sys.argv", ["build.py", "--sdk", str(sdk), "--out", str(out)]), \
                        patch.object(builder.subprocess, "run", side_effect=run):
                    if valid:
                        builder.main()
                    else:
                        with self.assertRaises(SystemExit):
                            builder.main()
                self.assertIn("--target=ppc32-xbox360", calls[0])
                self.assertEqual(calls[1][-2:], ["-t", "title"])


if __name__ == "__main__":
    unittest.main()
