"""Tests for the safe MySQL baseline backup tool."""

from __future__ import annotations

from contextlib import redirect_stderr
from datetime import datetime
import gzip
import hashlib
import inspect
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
from typing import BinaryIO
import unittest
from unittest.mock import patch

from tools.baseline import mysql_backup
from tools.baseline.mysql_backup import build_dump_command, create_backup


class _FakeProcess:
    """Small Popen-compatible process used to exercise stdout streaming."""

    def __init__(self, payload: bytes, exit_code: int = 0) -> None:
        self.stdout: BinaryIO = io.BytesIO(payload)
        self.returncode: int | None = None
        self.terminated = False
        self._exit_code = exit_code

    def wait(self) -> int:
        self.returncode = -15 if self.terminated else self._exit_code
        return self.returncode

    def terminate(self) -> None:
        self.terminated = True


def _write_private_defaults(path: Path) -> None:
    path.write_text(
        "[client]\nuser=backup\npassword=super-secret\n", encoding="utf-8"
    )
    path.chmod(0o600)


class MySqlBackupTests(unittest.TestCase):
    """Exercise command construction, backup creation, and failure cleanup."""

    def test_dump_command_places_defaults_file_first(self) -> None:
        module_functions = [
            value
            for value in vars(mysql_backup).values()
            if callable(value)
            and getattr(value, "__module__", None) == mysql_backup.__name__
        ]
        self.assertTrue(module_functions)
        self.assertTrue(
            all(function.__doc__ for function in module_functions),
            [
                function.__name__
                for function in module_functions
                if not function.__doc__
            ],
        )
        protocol_methods = [
            value
            for protocol in (mysql_backup.DumpProcess, mysql_backup.PopenFactory)
            for value in vars(protocol).values()
            if inspect.isfunction(value)
            and value.__module__ == mysql_backup.__name__
        ]
        self.assertTrue(
            all(method.__doc__ for method in protocol_methods),
            [method.__qualname__ for method in protocol_methods if not method.__doc__],
        )
        self.assertTrue(
            all(
                parameter.annotation is not inspect.Parameter.empty
                for method in protocol_methods
                for parameter in inspect.signature(method).parameters.values()
            )
        )
        self.assertIn("verify", create_backup.__doc__ or "")
        self.assertIn("both", create_backup.__doc__ or "")
        self.assertEqual(
            build_dump_command(Path("mysql.cnf"), "NetDisk"),
            [
                "mysqldump",
                "--defaults-file=mysql.cnf",
                "--single-transaction",
                "--routines",
                "--triggers",
                "--events",
                "--hex-blob",
                "--databases",
                "NetDisk",
            ],
        )

        sql = b"-- deterministic dump\nCREATE TABLE sample (id INT);\n"
        fixed_time = datetime(2025, 1, 2, 3, 4, 5)
        commands: list[list[str]] = []
        environments: list[dict[str, str]] = []

        def successful_factory(
            command: list[str],
            *,
            stdout: int,
            stderr: int,
            env: dict[str, str],
        ) -> _FakeProcess:
            commands.append(command)
            environments.append(env)
            self.assertEqual(stdout, subprocess.PIPE)
            self.assertEqual(stderr, subprocess.DEVNULL)
            self.assertNotIn("MYSQL_PWD", env)
            login_file = Path(env["MYSQL_TEST_LOGIN_FILE"])
            self.assertFalse(login_file.exists())
            self.assertTrue(login_file.parent.name.startswith(".NetDisk-staging-"))
            return _FakeProcess(sql)

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            defaults_file = root / "mysql.cnf"
            _write_private_defaults(defaults_file)
            output_dir = root / "backups"
            warning_output = io.StringIO()

            with patch.dict(os.environ, {"MYSQL_PWD": "environment-secret"}):
                with redirect_stderr(warning_output):
                    archive = create_backup(
                        defaults_file,
                        "NetDisk",
                        output_dir,
                        now=fixed_time,
                        popen_factory=successful_factory,
                    )

            self.assertEqual(archive.name, "NetDisk-20250102-030405.sql.gz")
            self.assertEqual(gzip.decompress(archive.read_bytes()), sql)
            checksum_path = archive.with_suffix(archive.suffix + ".sha256")
            expected_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            self.assertEqual(
                checksum_path.read_text(encoding="ascii"),
                f"{expected_digest}  {archive.name}\n",
            )
            self.assertEqual(
                sorted(path.name for path in output_dir.iterdir()),
                [archive.name, checksum_path.name],
            )
            self.assertEqual(len(commands), 1)
            self.assertEqual(len(environments), 1)
            self.assertNotIn("super-secret", " ".join(commands[0]))
            self.assertNotIn("environment-secret", repr(environments[0]))
            self.assertEqual(list(root.rglob("*.sql")), [])
            if os.name == "posix":
                self.assertEqual(output_dir.stat().st_mode & 0o777, 0o700)
                self.assertEqual(archive.stat().st_mode & 0o777, 0o600)
                self.assertEqual(checksum_path.stat().st_mode & 0o777, 0o600)
            else:
                self.assertIn(
                    "Windows ACL validation is unavailable",
                    warning_output.getvalue(),
                )

    def test_backup_rejects_missing_defaults_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            output_dir = root / "backups"

            with self.assertRaises(FileNotFoundError):
                create_backup(root / "missing.cnf", "NetDisk", output_dir)
            self.assertFalse(output_dir.exists())

            with self.assertRaises(ValueError):
                build_dump_command(Path("mysql.cnf"), "invalid-name")

            defaults_file = root / "mysql.cnf"
            _write_private_defaults(defaults_file)

            def missing_factory(
                command: list[str],
                *,
                stdout: int,
                stderr: int,
                env: dict[str, str],
            ) -> _FakeProcess:
                raise FileNotFoundError("mysqldump executable missing")

            with self.assertRaisesRegex(FileNotFoundError, "executable missing"):
                create_backup(
                    defaults_file,
                    "NetDisk",
                    output_dir,
                    popen_factory=missing_factory,
                )
            self.assertEqual(list(output_dir.iterdir()), [])

            if os.name == "posix":
                insecure_defaults = root / "insecure.cnf"
                _write_private_defaults(insecure_defaults)
                insecure_defaults.chmod(0o640)
                with self.assertRaises(PermissionError):
                    create_backup(insecure_defaults, "NetDisk", root / "not-created")

                linked_defaults = root / "linked.cnf"
                linked_defaults.symlink_to(defaults_file)
                with self.assertRaises(ValueError):
                    create_backup(linked_defaults, "NetDisk", root / "not-created")

                insecure_output = root / "insecure-output"
                insecure_output.mkdir(mode=0o700)
                insecure_output.chmod(0o750)
                with self.assertRaises(PermissionError):
                    create_backup(defaults_file, "NetDisk", insecure_output)

                link_target = root / "link-target"
                link_target.mkdir(mode=0o700)
                linked_output = root / "linked-output"
                linked_output.symlink_to(link_target, target_is_directory=True)
                with self.assertRaises(ValueError):
                    create_backup(defaults_file, "NetDisk", linked_output)
            else:
                junction_target = root / "junction-target"
                junction_target.mkdir()
                junction_defaults = junction_target / "mysql.cnf"
                _write_private_defaults(junction_defaults)
                junction_path = root / "junction-output"
                junction_result = subprocess.run(
                    [
                        "cmd",
                        "/c",
                        "mklink",
                        "/J",
                        str(junction_path),
                        str(junction_target),
                    ],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if junction_result.returncode == 0:
                    try:
                        with self.assertRaises(ValueError):
                            create_backup(
                                junction_path / "mysql.cnf",
                                "NetDisk",
                                root / "junction-defaults-output",
                                popen_factory=missing_factory,
                            )
                        with self.assertRaises(ValueError):
                            create_backup(defaults_file, "NetDisk", junction_path)
                    finally:
                        os.rmdir(junction_path)
                else:
                    self.skipTest(
                        f"junction creation unavailable: {junction_result.stderr}"
                    )

            project_root = Path(__file__).resolve().parents[2]
            help_result = subprocess.run(
                [sys.executable, "-m", "tools.baseline.mysql_backup", "--help"],
                cwd=project_root,
                capture_output=True,
                text=True,
                check=False,
            )
            missing_result = subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "tools.baseline.mysql_backup",
                    "--defaults-file",
                    str(root / "cli-missing.cnf"),
                    "--output-dir",
                    str(root / "cli-output"),
                ],
                cwd=project_root,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(help_result.returncode, 0, help_result.stderr)
            self.assertIn("[client]", help_result.stdout)
            self.assertIn("chmod600", help_result.stdout)
            self.assertIn("both", help_result.stdout)
            self.assertIn("SHA-256", help_result.stdout)
            self.assertEqual(missing_result.returncode, 1)
            self.assertNotIn("Traceback", missing_result.stderr)

    def test_failed_dump_leaves_no_final_archive(self) -> None:
        captured_environments: list[dict[str, str]] = []

        def failed_factory(
            command: list[str],
            *,
            stdout: int,
            stderr: int,
            env: dict[str, str],
        ) -> _FakeProcess:
            captured_environments.append(env)
            self.assertEqual(stderr, subprocess.DEVNULL)
            return _FakeProcess(b"partial secret SQL\n", exit_code=2)

        def successful_factory(
            command: list[str],
            *,
            stdout: int,
            stderr: int,
            env: dict[str, str],
        ) -> _FakeProcess:
            return _FakeProcess(b"SELECT 1;\n")

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            defaults_file = root / "mysql.cnf"
            _write_private_defaults(defaults_file)
            failed_output_dir = root / "failed"

            with patch.dict(os.environ, {"MYSQL_PWD": "top secret fragment-seal"}):
                with self.assertRaises(RuntimeError) as caught:
                    create_backup(
                        defaults_file,
                        "NetDisk",
                        failed_output_dir,
                        now=datetime(2025, 1, 2, 3, 4, 5),
                        popen_factory=failed_factory,
                    )

            self.assertIn("dump failed", str(caught.exception))
            for secret_fragment in ("top", "secret", "fragment-seal"):
                with self.subTest(secret_fragment=secret_fragment):
                    self.assertNotIn(secret_fragment, str(caught.exception))
            self.assertNotIn("MYSQL_PWD", captured_environments[0])
            self.assertEqual(list(failed_output_dir.iterdir()), [])
            self.assertEqual(list(root.rglob("*.sql")), [])

            class FailingStream(io.BytesIO):
                def read(self, size: int = -1) -> bytes:
                    raise OSError("simulated stdout read failure")

            streaming_process = _FakeProcess(b"")
            streaming_process.stdout = FailingStream()

            def streaming_failure_factory(
                command: list[str],
                *,
                stdout: int,
                stderr: int,
                env: dict[str, str],
            ) -> _FakeProcess:
                return streaming_process

            streaming_failure_output = root / "streaming-failure"
            with self.assertRaisesRegex(OSError, "stdout read failure"):
                create_backup(
                    defaults_file,
                    "NetDisk",
                    streaming_failure_output,
                    now=datetime(2025, 1, 2, 3, 4, 5),
                    popen_factory=streaming_failure_factory,
                )
            self.assertTrue(streaming_process.terminated)
            self.assertEqual(streaming_process.returncode, -15)
            self.assertEqual(list(streaming_failure_output.iterdir()), [])

            fixed_time = datetime(2025, 1, 2, 3, 4, 5)
            archive_name = "NetDisk-20250102-030405.sql.gz"
            checksum_name = f"{archive_name}.sha256"
            preexisting_output_dir = root / "preexisting"
            preexisting_output_dir.mkdir(mode=0o700)
            preexisting_output_dir.chmod(0o700)
            preexisting_archive = preexisting_output_dir / archive_name
            preexisting_checksum = preexisting_output_dir / checksum_name
            preexisting_archive.write_bytes(b"valid existing archive")
            preexisting_checksum.write_bytes(b"valid existing checksum")

            with self.assertRaises(FileExistsError):
                create_backup(
                    defaults_file,
                    "NetDisk",
                    preexisting_output_dir,
                    now=fixed_time,
                    popen_factory=successful_factory,
                )
            self.assertEqual(
                preexisting_archive.read_bytes(), b"valid existing archive"
            )
            self.assertEqual(
                preexisting_checksum.read_bytes(), b"valid existing checksum"
            )

            archive_only_output_dir = root / "preexisting-archive-only"
            archive_only_output_dir.mkdir(mode=0o700)
            archive_only_output_dir.chmod(0o700)
            archive_only = archive_only_output_dir / archive_name
            archive_only_checksum = archive_only_output_dir / checksum_name
            archive_only.write_bytes(b"archive without checksum")

            with self.assertRaises(FileExistsError):
                create_backup(
                    defaults_file,
                    "NetDisk",
                    archive_only_output_dir,
                    now=fixed_time,
                    popen_factory=successful_factory,
                )
            self.assertEqual(archive_only.read_bytes(), b"archive without checksum")
            self.assertFalse(archive_only_checksum.exists())

            competitor_output_dir = root / "competitor"
            competitor_checksum = competitor_output_dir / checksum_name
            real_link = mysql_backup.os.link

            def publish_then_compete(source: Path, destination: Path) -> None:
                real_link(source, destination)
                destination.unlink()
                destination.write_bytes(b"competitor checksum")
                raise OSError("simulated competing replacement")

            with patch.object(
                mysql_backup.os, "link", side_effect=publish_then_compete
            ):
                with self.assertRaisesRegex(OSError, "competing replacement"):
                    create_backup(
                        defaults_file,
                        "NetDisk",
                        competitor_output_dir,
                        now=fixed_time,
                        popen_factory=successful_factory,
                    )
            self.assertEqual(competitor_checksum.read_bytes(), b"competitor checksum")
            self.assertEqual(
                [path.name for path in competitor_output_dir.iterdir()], [checksum_name]
            )

            archive_competitor_output = root / "archive-competitor"
            archive_competitor_archive = archive_competitor_output / archive_name
            archive_competitor_checksum = archive_competitor_output / checksum_name
            link_count = 0

            def compete_with_archive(source: Path, destination: Path) -> None:
                nonlocal link_count
                link_count += 1
                real_link(source, destination)
                if link_count == 2:
                    destination.unlink()
                    destination.write_bytes(b"competitor archive")
                    raise OSError("simulated archive replacement")

            with patch.object(
                mysql_backup.os, "link", side_effect=compete_with_archive
            ):
                with self.assertRaisesRegex(OSError, "archive replacement"):
                    create_backup(
                        defaults_file,
                        "NetDisk",
                        archive_competitor_output,
                        now=fixed_time,
                        popen_factory=successful_factory,
                    )
            self.assertEqual(
                archive_competitor_archive.read_bytes(), b"competitor archive"
            )
            self.assertFalse(archive_competitor_checksum.exists())

            cleanup_output_dir = root / "cleanup-failure"
            cleanup_archive = cleanup_output_dir / archive_name
            cleanup_checksum = cleanup_output_dir / checksum_name
            real_unlink = Path.unlink

            def refuse_archive_unlink(path: Path, missing_ok: bool = False) -> None:
                if path == cleanup_archive:
                    raise PermissionError("archive cleanup blocked")
                real_unlink(path, missing_ok=missing_ok)

            with patch.object(
                mysql_backup,
                "_fsync_directory",
                side_effect=[None, None, OSError("final cleanup fsync failed"), None],
            ):
                with patch.object(Path, "unlink", new=refuse_archive_unlink):
                    with self.assertRaisesRegex(
                        OSError, "final cleanup fsync failed"
                    ) as cleanup_error:
                        create_backup(
                            defaults_file,
                            "NetDisk",
                            cleanup_output_dir,
                            now=fixed_time,
                            popen_factory=successful_factory,
                        )
            self.assertTrue(cleanup_archive.is_file())
            self.assertTrue(cleanup_checksum.is_file())
            self.assertTrue(
                any(
                    "archive cleanup blocked" in note
                    for note in getattr(cleanup_error.exception, "__notes__", [])
                )
            )

            crash_output_dir = root / "crash"
            crash_script = textwrap.dedent(
                f"""
                from datetime import datetime
                import io
                import os
                from pathlib import Path
                from tools.baseline import mysql_backup

                class Process:
                    def __init__(self):
                        self.stdout = io.BytesIO(b"SELECT 1;\\n")
                        self.returncode = None
                    def wait(self):
                        self.returncode = 0
                        return 0
                    def terminate(self):
                        self.returncode = -15

                def factory(command, *, stdout, stderr, env):
                    return Process()

                real_link = mysql_backup.os.link
                def crash_after_first_link(source, destination):
                    real_link(source, destination)
                    os._exit(73)
                mysql_backup.os.link = crash_after_first_link
                mysql_backup.create_backup(
                    Path({str(defaults_file)!r}),
                    "NetDisk",
                    Path({str(crash_output_dir)!r}),
                    now=datetime(2025, 1, 2, 3, 4, 5),
                    popen_factory=factory,
                )
                """
            )
            crash_result = subprocess.run(
                [sys.executable, "-c", crash_script],
                cwd=Path(__file__).resolve().parents[2],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(crash_result.returncode, 73, crash_result.stderr)
            self.assertFalse((crash_output_dir / archive_name).exists())
            self.assertTrue((crash_output_dir / checksum_name).is_file())
            self.assertEqual(list(crash_output_dir.rglob("*.sql")), [])
            staging_directories = [
                path for path in crash_output_dir.iterdir() if path.is_dir()
            ]
            self.assertEqual(len(staging_directories), 1)
            staging_directory = staging_directories[0]
            self.assertTrue(staging_directory.name.startswith(".NetDisk-staging-"))
            self.assertTrue(
                all(
                    path.name.endswith((".sql.gz", ".sha256"))
                    for path in staging_directory.iterdir()
                )
            )
            if os.name == "posix":
                self.assertEqual(staging_directory.stat().st_mode & 0o777, 0o700)


if __name__ == "__main__":
    unittest.main()
