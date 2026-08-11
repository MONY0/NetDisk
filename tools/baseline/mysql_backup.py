"""Create confidential, deterministic compressed MySQL baseline backups."""

from __future__ import annotations

import argparse
from collections.abc import Callable, Sequence
from datetime import datetime
import gzip
import hashlib
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
from typing import BinaryIO, Protocol


_COPY_BUFFER_SIZE = 1024 * 1024
_DATABASE_PATTERN = re.compile(r"[A-Za-z0-9_]+\Z")
_FileIdentity = tuple[int, int]


class DumpProcess(Protocol):
    """Minimal binary subprocess interface required by the backup stream."""

    stdout: BinaryIO | None
    returncode: int | None

    def wait(self: DumpProcess) -> int:
        """Wait for the process and return its exit code."""

    def terminate(self: DumpProcess) -> None:
        """Request process termination."""


class PopenFactory(Protocol):
    """Callable interface used to start mysqldump."""

    def __call__(
        self: PopenFactory,
        command: list[str],
        *,
        stdout: int,
        stderr: int,
        env: dict[str, str],
    ) -> DumpProcess:
        """Start mysqldump with binary pipes and a sanitized environment."""
        ...


def _validate_database(database: str) -> None:
    """Reject database names that cannot be safely passed as one argument."""
    if _DATABASE_PATTERN.fullmatch(database) is None:
        raise ValueError(
            "database name must be nonempty and contain only letters, digits, "
            "or underscores"
        )


def _validate_defaults_file(defaults_file: Path) -> Path:
    """Resolve and validate a private, current-user MySQL option file."""
    requested = _reject_link_path_chain(defaults_file)
    status = requested.lstat()
    if not stat.S_ISREG(status.st_mode):
        raise ValueError(f"defaults file is not a regular file: {requested}")
    resolved = requested.resolve(strict=True)
    status = resolved.stat()
    if not stat.S_ISREG(status.st_mode):
        raise ValueError(f"defaults file is not a regular file: {resolved}")
    if os.name == "posix":
        if status.st_uid != os.getuid():
            raise PermissionError(
                f"defaults file is not owned by the current user: {resolved}"
            )
        if stat.S_IMODE(status.st_mode) & 0o077:
            raise PermissionError(
                f"defaults file permissions must be 600 or stricter: {resolved}"
            )
    return resolved


def _is_link_or_reparse(path: Path) -> bool:
    """Return whether an existing path is a symlink or Windows reparse point."""
    try:
        status = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(status.st_mode):
        return True
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    file_attributes = getattr(status, "st_file_attributes", 0)
    return bool(reparse_flag and file_attributes & reparse_flag)


def _reject_link_path_chain(path: Path) -> Path:
    """Return an absolute path after rejecting links in its existing chain."""
    expanded = path.expanduser()
    if ".." in expanded.parts:
        raise ValueError("output directory path must not contain '..'")
    absolute = expanded if expanded.is_absolute() else Path.cwd() / expanded
    parts = absolute.parts
    current = Path(parts[0])
    if _is_link_or_reparse(current):
        raise ValueError(
            f"output directory path contains a link or reparse point: {current}"
        )
    for part in parts[1:]:
        current /= part
        if _is_link_or_reparse(current):
            raise ValueError(
                f"output directory path contains a link or reparse point: {current}"
            )
    return absolute


def _prepare_output_directory(output_dir: Path) -> Path:
    """Create or validate a private output directory owned by this user."""
    requested = _reject_link_path_chain(output_dir)
    existed = requested.exists()
    requested.mkdir(mode=0o700, parents=True, exist_ok=True)
    if not existed and os.name == "posix":
        requested.chmod(0o700)
    _reject_link_path_chain(requested)
    canonical = requested.resolve(strict=True)
    status = canonical.stat()
    if not stat.S_ISDIR(status.st_mode):
        raise ValueError(f"output path is not a directory: {canonical}")
    if os.name == "posix":
        if status.st_uid != os.getuid():
            raise PermissionError(
                f"output directory is not owned by the current user: {canonical}"
            )
        if stat.S_IMODE(status.st_mode) & 0o077:
            raise PermissionError(
                f"output directory permissions must be 700 or stricter: {canonical}"
            )
    return canonical


def _regular_file_identity(path: Path) -> _FileIdentity | None:
    """Return a regular file's device/inode identity without following links."""
    try:
        status = path.lstat()
    except FileNotFoundError:
        return None
    if not stat.S_ISREG(status.st_mode):
        return None
    return status.st_dev, status.st_ino


def _publish_no_overwrite(
    temporary_path: Path,
    destination: Path,
    expected_identity: _FileIdentity,
) -> None:
    """Hard-link a verified staging file to a new final path without overwrite."""
    if _regular_file_identity(temporary_path) != expected_identity:
        raise RuntimeError("temporary backup file identity changed")
    os.link(temporary_path, destination)
    if _regular_file_identity(destination) != expected_identity:
        raise RuntimeError("published backup file identity changed")


def _unlink_if_owned(
    destination: Path, expected_identity: _FileIdentity | None
) -> bool:
    """Unlink a path only when it still names the file created by this run."""
    if (
        expected_identity is not None
        and _regular_file_identity(destination) == expected_identity
    ):
        destination.unlink()
        return True
    return False


def _unlink_checksum_if_no_owned_archive(
    checksum: Path,
    checksum_identity: _FileIdentity | None,
    archive: Path,
    archive_identity: _FileIdentity | None,
) -> bool:
    """Remove an owned checksum unless this run's final archive remains."""
    if (
        archive_identity is not None
        and _regular_file_identity(archive) == archive_identity
    ):
        return False
    return _unlink_if_owned(checksum, checksum_identity)


def _fsync_directory(directory: Path) -> None:
    """Persist POSIX directory-entry changes; do nothing on development Windows."""
    if os.name != "posix":
        return
    descriptor = os.open(directory, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _sha256_file(path: Path) -> str:
    """Return the lowercase SHA-256 digest of a file's bytes."""
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(_COPY_BUFFER_SIZE):
            digest.update(block)
    return digest.hexdigest()


def _open_private_binary(path: Path) -> BinaryIO:
    """Exclusively create a binary staging file with mode 0600."""
    descriptor = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    return os.fdopen(descriptor, "wb")


def _cleanup_action(
    original_error: BaseException,
    description: str,
    action: Callable[[], object],
) -> None:
    """Attempt one cleanup action and attach any failure to the original error."""
    try:
        action()
    except BaseException as cleanup_error:
        original_error.add_note(f"cleanup failed ({description}): {cleanup_error}")


def _warn_windows_acl_limitation() -> None:
    """Warn that Windows development runs cannot validate restrictive ACLs."""
    if os.name == "nt":
        print(
            "warning: Windows ACL validation is unavailable; "
            "the supported production target is a protected Linux directory",
            file=sys.stderr,
        )


def build_dump_command(
    defaults_file: Path, database: str, executable: str = "mysqldump"
) -> list[str]:
    """Build a mysqldump command that reads only the named option file."""
    _validate_database(database)
    return [
        executable,
        f"--defaults-file={defaults_file}",
        "--single-transaction",
        "--routines",
        "--triggers",
        "--events",
        "--hex-blob",
        "--databases",
        database,
    ]


def create_backup(
    defaults_file: Path,
    database: str,
    output_dir: Path,
    *,
    now: datetime | None = None,
    popen_factory: PopenFactory = subprocess.Popen,
) -> Path:
    """Stream mysqldump into gzip and commit a checksum/archive pair safely.

    Consumers must require both returned archive and its adjacent ``.sha256``
    file, then verify the archive against that checksum before using it.
    """
    _validate_database(database)
    resolved_defaults = _validate_defaults_file(defaults_file)
    canonical_output = _prepare_output_directory(output_dir)
    timestamp = (now or datetime.now()).strftime("%Y%m%d-%H%M%S")
    archive = canonical_output / f"{database}-{timestamp}.sql.gz"
    checksum = archive.with_suffix(archive.suffix + ".sha256")

    staging_directory: Path | None = None
    gzip_path: Path | None = None
    checksum_temp_path: Path | None = None
    archive_identity: _FileIdentity | None = None
    checksum_identity: _FileIdentity | None = None
    process: DumpProcess | None = None
    process_waited = False
    try:
        staging_directory = Path(
            tempfile.mkdtemp(prefix=f".{database}-staging-", dir=canonical_output)
        )
        if os.name == "posix":
            staging_directory.chmod(0o700)
        login_file = staging_directory / "mysql-login-disabled.cnf"
        if login_file.exists():
            raise RuntimeError("private login-file sentinel unexpectedly exists")

        environment = os.environ.copy()
        environment.pop("MYSQL_PWD", None)
        environment["MYSQL_TEST_LOGIN_FILE"] = str(login_file)
        process = popen_factory(
            build_dump_command(resolved_defaults, database),
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            env=environment,
        )
        if process.stdout is None:
            raise RuntimeError("mysqldump stdout pipe is unavailable")

        gzip_path = staging_directory / f"{archive.name}.tmp.sql.gz"
        with _open_private_binary(gzip_path) as gzip_file:
            with process.stdout as dump_stdout:
                with gzip.GzipFile(
                    filename="", mode="wb", fileobj=gzip_file, mtime=0
                ) as compressed_file:
                    while block := dump_stdout.read(_COPY_BUFFER_SIZE):
                        compressed_file.write(block)
            gzip_file.flush()
            os.fsync(gzip_file.fileno())

        exit_code = process.wait()
        process_waited = True
        if exit_code != 0:
            raise RuntimeError(
                f"mysqldump failed with exit code {exit_code}; stderr suppressed"
            )

        archive_identity = _regular_file_identity(gzip_path)
        if archive_identity is None:
            raise RuntimeError("temporary archive is not a regular file")

        checksum_temp_path = staging_directory / f"{archive.name}.tmp.sha256"
        checksum_line = f"{_sha256_file(gzip_path)}  {archive.name}\n".encode("ascii")
        with _open_private_binary(checksum_temp_path) as checksum_file:
            checksum_file.write(checksum_line)
            checksum_file.flush()
            os.fsync(checksum_file.fileno())
        checksum_identity = _regular_file_identity(checksum_temp_path)
        if checksum_identity is None:
            raise RuntimeError("temporary checksum is not a regular file")

        _publish_no_overwrite(checksum_temp_path, checksum, checksum_identity)
        _fsync_directory(canonical_output)
        if _regular_file_identity(checksum) != checksum_identity:
            raise RuntimeError("published checksum identity changed")

        _publish_no_overwrite(gzip_path, archive, archive_identity)
        _fsync_directory(canonical_output)
        if _regular_file_identity(archive) != archive_identity:
            raise RuntimeError("published archive identity changed")
        if _regular_file_identity(checksum) != checksum_identity:
            raise RuntimeError("published checksum identity changed")

        result_path = archive.absolute()
        gzip_path.unlink()
        gzip_path = None
        checksum_temp_path.unlink()
        checksum_temp_path = None
        staging_directory.rmdir()
        staging_directory = None
        _fsync_directory(canonical_output)

        if _regular_file_identity(archive) != archive_identity:
            raise RuntimeError("committed archive identity changed")
        if _regular_file_identity(checksum) != checksum_identity:
            raise RuntimeError("committed checksum identity changed")
        _warn_windows_acl_limitation()
        return result_path
    except BaseException as error:
        if process is not None and not process_waited:
            _cleanup_action(error, "terminate mysqldump", process.terminate)
            if process.stdout is not None:
                _cleanup_action(error, "close mysqldump stdout", process.stdout.close)
            _cleanup_action(error, "wait for mysqldump", process.wait)

        _cleanup_action(
            error,
            "remove owned archive",
            lambda: _unlink_if_owned(archive, archive_identity),
        )
        _cleanup_action(
            error,
            "remove owned checksum",
            lambda: _unlink_checksum_if_no_owned_archive(
                checksum, checksum_identity, archive, archive_identity
            ),
        )
        if gzip_path is not None:
            _cleanup_action(
                error, "remove compressed staging file", gzip_path.unlink
            )
        if checksum_temp_path is not None:
            _cleanup_action(
                error, "remove checksum staging file", checksum_temp_path.unlink
            )
        if staging_directory is not None:
            _cleanup_action(
                error, "remove private staging directory", staging_directory.rmdir
            )
        _cleanup_action(
            error,
            "fsync output directory after cleanup",
            lambda: _fsync_directory(canonical_output),
        )
        raise


def _build_parser() -> argparse.ArgumentParser:
    """Build the command-line parser for the backup utility."""
    parser = argparse.ArgumentParser(
        description="Create a compressed MySQL baseline backup.",
        epilog=(
            "Consumers must keep both output files and verify the archive's "
            "SHA-256 checksum before use."
        ),
    )
    parser.add_argument(
        "--defaults-file",
        type=Path,
        required=True,
        help="exclusive MySQL [client] defaults file with credentials (use chmod600)",
    )
    parser.add_argument(
        "--database", default="NetDisk", help="database name (default: NetDisk)"
    )
    parser.add_argument(
        "--output-dir", type=Path, required=True, help="private backup directory"
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Run the command-line backup utility and return its process status."""
    arguments = _build_parser().parse_args(argv)
    try:
        archive = create_backup(
            arguments.defaults_file, arguments.database, arguments.output_dir
        )
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(archive)
    print(archive.with_suffix(archive.suffix + ".sha256"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
