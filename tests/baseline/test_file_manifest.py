"""Tests for deterministic backup file manifests."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.baseline import file_manifest
from tools.baseline.file_manifest import (
    build_manifest,
    create_manifest,
    verify_manifest,
    write_manifest,
)


class FileManifestTests(unittest.TestCase):
    """Exercise manifest creation, persistence, and verification."""

    def test_build_manifest_records_hashes_and_sorted_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory, tempfile.TemporaryDirectory() as outside_directory:
            root = Path(temporary_directory)
            outside = Path(outside_directory)
            (root / "z.txt").write_bytes(b"z")
            nested = root / "nested"
            nested.mkdir()
            (nested / "a.txt").write_bytes(b"abc")
            (outside / "secret.txt").write_bytes(b"secret")
            linked_directory = root / "linked-outside"
            if os.name == "nt":
                result = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(linked_directory), str(outside)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
            else:
                try:
                    linked_directory.symlink_to(outside, target_is_directory=True)
                except OSError as error:
                    self.skipTest(f"directory symlinks are unavailable: {error}")

            try:
                manifest = build_manifest(root)
            finally:
                if os.name == "nt":
                    os.rmdir(linked_directory)
                else:
                    linked_directory.unlink()

            files = manifest["files"]
            self.assertEqual([record["path"] for record in files], ["nested/a.txt", "z.txt"])
            self.assertNotIn("linked-outside/secret.txt", [record["path"] for record in files])
            self.assertEqual(files[0]["size"], 3)
            self.assertEqual(files[0]["md5"], hashlib.md5(b"abc").hexdigest())
            self.assertEqual(files[0]["sha256"], hashlib.sha256(b"abc").hexdigest())

            snapshot_root = root / "snapshot"
            snapshot_root.mkdir()
            watched_path = snapshot_root / "watched.txt"
            watched_path.write_bytes(b"old")
            before_mutation = watched_path.stat()
            original_fstat = file_manifest.os.fstat
            fstat_calls = 0

            def fstat_then_mutate(descriptor: int) -> os.stat_result:
                nonlocal fstat_calls
                snapshot = original_fstat(descriptor)
                fstat_calls += 1
                if fstat_calls == 1:
                    watched_path.write_bytes(b"new")
                    os.utime(
                        watched_path,
                        ns=(before_mutation.st_atime_ns, before_mutation.st_mtime_ns),
                    )
                return snapshot

            with patch.object(file_manifest.os, "fstat", side_effect=fstat_then_mutate):
                with self.assertRaises(ValueError):
                    build_manifest(snapshot_root)

            blocked_directory = root / "blocked"
            blocked_directory.mkdir()
            (blocked_directory / "secret.txt").write_bytes(b"secret")
            real_scandir = file_manifest.os.scandir

            def blocked_scandir(path: str | bytes | int) -> os.ScandirIterator[str]:
                if Path(path) == blocked_directory:
                    raise PermissionError("blocked directory")
                return real_scandir(path)

            with patch.object(file_manifest.os, "scandir", side_effect=blocked_scandir):
                with self.assertRaises(PermissionError):
                    build_manifest(root)

    def test_verify_manifest_detects_changed_missing_and_extra_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "a.txt").write_bytes(b"before")
            (root / "missing.txt").write_bytes(b"remove me")
            manifest_path = root / "manifest.json"
            write_manifest(build_manifest(root), manifest_path)

            (root / "a.txt").write_bytes(b"after")
            (root / "missing.txt").unlink()
            (root / "extra.txt").write_bytes(b"new")

            self.assertEqual(
                verify_manifest(root, manifest_path),
                ["changed:a.txt", "extra:extra.txt", "missing:missing.txt"],
            )
            boolean_version_path = root / "boolean-version.json"
            boolean_version_path.write_text(
                json.dumps({"version": True, "files": []}), encoding="utf-8"
            )
            with self.assertRaises(ValueError):
                verify_manifest(root, boolean_version_path)

            valid_record = {
                "path": "a.txt",
                "size": 1,
                "md5": hashlib.md5(b"x").hexdigest(),
                "sha256": hashlib.sha256(b"x").hexdigest(),
            }
            malformed_path = root / "malformed.json"
            malformed_path.write_text(
                json.dumps({"version": 1, "files": {}}), encoding="utf-8"
            )
            with self.assertRaises(ValueError):
                verify_manifest(root, malformed_path)
            malformed_path.write_text(
                json.dumps({"version": 1, "files": [valid_record, valid_record]}),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                verify_manifest(root, malformed_path)
            malformed_path.write_text(
                '{"version": 1, "version": 1, "files": []}', encoding="utf-8"
            )
            with self.assertRaises(ValueError):
                verify_manifest(root, malformed_path)
            for unsafe_path in (".", "./a", "a//b", "line\nfeed"):
                with self.subTest(unsafe_path=unsafe_path):
                    malformed_path.write_text(
                        json.dumps(
                            {"version": 1, "files": [{**valid_record, "path": unsafe_path}]}
                        ),
                        encoding="utf-8",
                    )
                    with self.assertRaises(ValueError):
                        verify_manifest(root, malformed_path)

    def test_write_manifest_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "a.txt").write_bytes(b"content")
            manifest = build_manifest(root)
            first_path = root / "first.json"
            second_path = root / "second.json"

            write_manifest(manifest, first_path)
            write_manifest(manifest, second_path)

            self.assertEqual(first_path.read_bytes(), second_path.read_bytes())
            self.assertEqual(json.loads(first_path.read_text(encoding="utf-8"))["version"], 1)

            repeated_root = root / "repeated"
            repeated_root.mkdir()
            (repeated_root / "keep.txt").write_bytes(b"keep")
            repeated_manifest_path = repeated_root / "manifest.json"
            self.assertEqual(
                create_manifest(repeated_root, repeated_manifest_path),
                repeated_manifest_path.resolve(),
            )
            create_manifest(repeated_root, repeated_manifest_path)
            self.assertEqual(verify_manifest(repeated_root, repeated_manifest_path), [])

            atomic_path = root / "atomic.json"
            with patch.object(file_manifest.os, "replace", side_effect=OSError("blocked")):
                with self.assertRaises(OSError):
                    write_manifest(manifest, atomic_path)
            self.assertEqual(list(root.glob(".atomic.json.*.tmp")), [])

            cli_root = root / "cli"
            cli_root.mkdir()
            cli_file = cli_root / "sample.txt"
            cli_file.write_bytes(b"before")
            cli_manifest_path = cli_root / "manifest.json"
            project_root = Path(__file__).resolve().parents[2]

            def run_cli(*arguments: str) -> subprocess.CompletedProcess[str]:
                return subprocess.run(
                    [sys.executable, "-m", "tools.baseline.file_manifest", *arguments],
                    cwd=project_root,
                    capture_output=True,
                    text=True,
                    check=False,
                )

            first_create = run_cli("create", str(cli_root), str(cli_manifest_path))
            second_create = run_cli("create", str(cli_root), str(cli_manifest_path))
            clean_verify = run_cli("verify", str(cli_root), str(cli_manifest_path))
            self.assertEqual(first_create.returncode, 0, first_create.stderr)
            self.assertEqual(second_create.returncode, 0, second_create.stderr)
            self.assertEqual(clean_verify.returncode, 0, clean_verify.stderr)
            self.assertEqual(clean_verify.stdout.strip(), "OK")
            cli_file.write_bytes(b"after")
            drift_verify = run_cli("verify", str(cli_root), str(cli_manifest_path))
            self.assertEqual(drift_verify.returncode, 1, drift_verify.stderr)
            self.assertEqual(drift_verify.stdout.strip(), "changed:sample.txt")
            malformed_cli_path = cli_root / "invalid.json"
            malformed_cli_path.write_text(
                json.dumps({"version": True, "files": []}), encoding="utf-8"
            )
            malformed_verify = run_cli("verify", str(cli_root), str(malformed_cli_path))
            self.assertEqual(malformed_verify.returncode, 2)
            self.assertIn("unsupported manifest version", malformed_verify.stderr)
            self.assertNotIn("Traceback", malformed_verify.stderr)


if __name__ == "__main__":
    unittest.main()
