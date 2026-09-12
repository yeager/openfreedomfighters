"""The private install-verification runner must never become a data exporter."""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys
import unittest
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import private_install_verification_runner as runner  # noqa: E402


class PrivateInstallVerificationRunnerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = ROOT / ".test-work" / "private-install-verification"
        shutil.rmtree(self.root, ignore_errors=True)
        self.root.mkdir(parents=True, mode=0o700)
        # Unit fixtures are deliberately project-authored and live in the
        # checkout.  Move only the runner's repository boundary for those
        # fixtures; the dedicated test below restores and exercises it.
        self.repository_root = runner.REPOSITORY_ROOT
        runner.REPOSITORY_ROOT = ROOT / "repository-boundary-not-a-fixture"

    def tearDown(self) -> None:
        runner.REPOSITORY_ROOT = self.repository_root
        shutil.rmtree(self.root, ignore_errors=True)

    def test_status_round_trip_contains_only_safe_structural_fields(self) -> None:
        workspace = runner._make_workspace(self.root / "run")
        descriptor = runner._open_workspace(workspace)
        try:
            runner._write_status(descriptor, runner._status_record("verified", 17))
        finally:
            runner.os.close(descriptor)
        self.assertEqual(runner._read_status(workspace), {
            "format": runner.FORMAT, "state": "verified", "elapsed_seconds": 17,
        })
        self.assertNotIn("data", (workspace / runner.STATUS_NAME).read_text())

    def test_status_rejects_a_symlink_without_reading_its_target(self) -> None:
        workspace = runner._make_workspace(self.root / "run")
        target = self.root / "retail-details"
        target.write_text('{"retail":"must not be read"}', encoding="utf-8")
        (workspace / runner.STATUS_NAME).symlink_to(target)
        with self.assertRaisesRegex(ValueError, "unavailable"):
            runner._read_status(workspace)

    def test_workspace_entry_symlink_is_rejected_before_resolution(self) -> None:
        target = self.root / "private-target"
        target.mkdir()
        link = self.root / "workspace-link"
        link.symlink_to(target.name)
        with self.assertRaisesRegex(ValueError, "must not be a symlink"):
            runner._outside_repository(link, "workspace")

    def test_run_never_captures_or_prints_child_output_and_scopes_cache(self) -> None:
        workspace = runner._make_workspace(self.root / "run")
        binary = self.root / "openfreedomfighters"
        binary.write_text("", encoding="utf-8")
        binary.chmod(0o700)
        data = self.root / "owned-data"
        data.mkdir()
        invoked: list[object] = []
        def fake_run(*args: object, **kwargs: object) -> subprocess.CompletedProcess[object]:
            invoked.extend((args, kwargs))
            return subprocess.CompletedProcess(args[0], 0)
        with mock.patch.object(runner.subprocess, "run", fake_run):
            result = runner._run(binary, data, workspace, 30)
        self.assertEqual(result["state"], "verified")
        command, kwargs = invoked[0][0], invoked[1]
        self.assertEqual(command[1:], ("--data", str(data), "--verify-only"))
        self.assertFalse(kwargs["shell"])
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)
        self.assertEqual(kwargs["stdin"], subprocess.DEVNULL)
        self.assertEqual(kwargs["cwd"], workspace)
        self.assertEqual(kwargs["env"]["XDG_CACHE_HOME"], str(workspace / "cache"))

    def test_launch_detaches_without_a_shell_and_status_survives_the_terminal(self) -> None:
        binary = self.root / "openfreedomfighters"
        binary.write_text("", encoding="utf-8")
        binary.chmod(0o700)
        data = self.root / "owned-data"
        data.mkdir()
        workspace = self.root / "run"
        invoked: list[object] = []
        def fake_popen(*args: object, **kwargs: object) -> object:
            invoked.extend((args, kwargs))
            return object()
        runner.launch(binary=binary, data_root=data, workspace=workspace,
                      timeout_seconds=30, popen=fake_popen)
        command, kwargs = invoked[0][0], invoked[1]
        self.assertIn("--execute", command)
        self.assertFalse(kwargs["shell"])
        self.assertTrue(kwargs["start_new_session"])
        self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
        self.assertEqual(runner._read_status(workspace)["state"], "running")

    def test_data_inside_repository_is_rejected_before_execution(self) -> None:
        runner.REPOSITORY_ROOT = self.repository_root
        try:
            with self.assertRaisesRegex(ValueError, "outside"):
                runner._validate_data_root(ROOT)
        finally:
            runner.REPOSITORY_ROOT = ROOT / "repository-boundary-not-a-fixture"


if __name__ == "__main__":
    unittest.main()
