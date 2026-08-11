"""Create and verify deterministic file backup manifests.

Live concurrent writers cannot provide a fully coherent scan on every filesystem.
Run this tool during a maintenance window or against a filesystem snapshot.
"""

from __future__ import annotations

import argparse
from collections.abc import Iterator, Sequence
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import tempfile
from typing import Any


CHUNK_SIZE = 1024 * 1024
_RECORD_KEYS = frozenset({"path", "size", "md5", "sha256"})


def _hash_file(path: Path) -> tuple[int, str, str]:
    """Return a stable size, MD5, and SHA-256 from one open file descriptor."""
    md5 = hashlib.md5(usedforsecurity=False)
    sha256 = hashlib.sha256()
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(path, flags)
    try:
        before = os.fstat(descriptor)
        if not stat.S_ISREG(before.st_mode):
            raise ValueError(f"not a regular file: {path}")
        while chunk := os.read(descriptor, CHUNK_SIZE):
            md5.update(chunk)
            sha256.update(chunk)
        after = os.fstat(descriptor)
        path_after = path.lstat()
    finally:
        os.close(descriptor)
    if (
        not stat.S_ISREG(path_after.st_mode)
        or not _same_file_snapshot(before, after)
        or not _same_file_identity(before, path_after)
    ):
        raise ValueError(f"file changed while hashing: {path}")
    return before.st_size, md5.hexdigest(), sha256.hexdigest()


def _same_file_snapshot(before: os.stat_result, after: os.stat_result) -> bool:
    """Return whether file metadata stayed stable while it was hashed."""
    return (
        before.st_dev,
        before.st_ino,
        before.st_mode,
        before.st_size,
        before.st_mtime_ns,
        getattr(before, "st_ctime_ns", None),
    ) == (
        after.st_dev,
        after.st_ino,
        after.st_mode,
        after.st_size,
        after.st_mtime_ns,
        getattr(after, "st_ctime_ns", None),
    )


def _same_file_identity(before: os.stat_result, path_after: os.stat_result) -> bool:
    """Return whether a pathname still identifies the descriptor's regular file."""
    return (
        before.st_dev,
        before.st_ino,
        before.st_mode,
        before.st_size,
        before.st_mtime_ns,
    ) == (
        path_after.st_dev,
        path_after.st_ino,
        path_after.st_mode,
        path_after.st_size,
        path_after.st_mtime_ns,
    )


def _is_directory_reparse_point(path: Path) -> bool:
    """Return whether *path* must not be traversed as a directory link."""
    path_stat = path.lstat()
    if stat.S_ISLNK(path_stat.st_mode):
        return True
    attributes = getattr(path_stat, "st_file_attributes", 0)
    reparse_point = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return bool(reparse_point and attributes & reparse_point)


def _resolve_root(root: Path | str) -> Path:
    """Resolve and validate a manifest root directory."""
    resolved_root = Path(root).resolve()
    if not stat.S_ISDIR(resolved_root.stat().st_mode):
        raise ValueError(f"root is not a directory: {resolved_root}")
    return resolved_root


def _absolute_destination(output: Path | str) -> Path:
    """Return an absolute output pathname without resolving symlinks."""
    return Path(os.path.abspath(os.fspath(output)))


def _ensure_safe_output_parent(destination: Path) -> None:
    """Reject output paths whose existing parent chain crosses a directory link."""
    parent = destination.parent
    while parent != parent.parent:
        try:
            if _is_directory_reparse_point(parent):
                raise ValueError(f"output path crosses a directory link: {parent}")
        except FileNotFoundError:
            pass
        parent = parent.parent


def _raise_walk_error(error: OSError) -> None:
    """Propagate a directory scan failure instead of producing an incomplete manifest."""
    raise error


def _iter_files(root: Path, excluded_paths: set[Path]) -> Iterator[Path]:
    """Yield non-symlink files below *root* in deterministic order."""
    for directory_text, directories, filenames in os.walk(
        root, followlinks=False, onerror=_raise_walk_error
    ):
        directory = Path(directory_text)
        directories[:] = sorted(
            name
            for name in directories
            if not _is_directory_reparse_point(directory / name)
        )
        for filename in sorted(filenames):
            candidate = directory / filename
            candidate_stat = candidate.lstat()
            if stat.S_ISLNK(candidate_stat.st_mode) or candidate in excluded_paths:
                continue
            if stat.S_ISREG(candidate_stat.st_mode):
                yield candidate


def _build_manifest(root: Path, excluded_paths: set[Path]) -> dict[str, Any]:
    """Build a manifest from *root*, excluding selected resolved files."""
    records: list[dict[str, Any]] = []
    for path in _iter_files(root, excluded_paths):
        size, md5, sha256 = _hash_file(path)
        records.append(
            {
                "path": path.relative_to(root).as_posix(),
                "size": size,
                "md5": md5,
                "sha256": sha256,
            }
        )
    records.sort(key=lambda record: str(record["path"]))
    return {"version": 1, "files": records}


def build_manifest(root: Path | str) -> dict[str, Any]:
    """Build a deterministic manifest of all regular, non-symlink files in *root*."""
    return _build_manifest(_resolve_root(root), set())


def _validate_record(record: Any) -> dict[str, Any]:
    """Validate one decoded manifest record and return it unchanged."""
    if not isinstance(record, dict) or set(record) != _RECORD_KEYS:
        raise ValueError("manifest file record is malformed")
    path = record["path"]
    size = record["size"]
    md5 = record["md5"]
    sha256 = record["sha256"]
    if not isinstance(path, str) or not path:
        raise ValueError("manifest path is malformed")
    pure_path = PurePosixPath(path)
    if (
        pure_path.is_absolute()
        or "\\" in path
        or pure_path.as_posix() != path
        or not pure_path.parts
        or any(part in {"", ".", ".."} for part in pure_path.parts)
        or any(ord(character) < 32 or ord(character) == 127 for character in path)
    ):
        raise ValueError("manifest path is not a safe relative POSIX path")
    if not isinstance(size, int) or isinstance(size, bool) or size < 0:
        raise ValueError("manifest file size is malformed")
    if not isinstance(md5, str) or len(md5) != 32 or not _is_hex(md5):
        raise ValueError("manifest MD5 digest is malformed")
    if not isinstance(sha256, str) or len(sha256) != 64 or not _is_hex(sha256):
        raise ValueError("manifest SHA-256 digest is malformed")
    return record


def _is_hex(value: str) -> bool:
    """Return whether *value* contains only hexadecimal characters."""
    return all(character in "0123456789abcdefABCDEF" for character in value)


def _validate_manifest(manifest: Any) -> dict[str, Any]:
    """Validate the strict on-disk manifest schema."""
    if not isinstance(manifest, dict) or set(manifest) != {"version", "files"}:
        raise ValueError("manifest is malformed")
    if type(manifest["version"]) is not int or manifest["version"] != 1:
        raise ValueError("unsupported manifest version")
    files = manifest["files"]
    if not isinstance(files, list):
        raise ValueError("manifest files must be a list")
    records = [_validate_record(record) for record in files]
    paths = [record["path"] for record in records]
    if paths != sorted(paths) or len(paths) != len(set(paths)):
        raise ValueError("manifest file paths must be unique and sorted")
    return {"version": 1, "files": records}


def write_manifest(manifest: dict[str, Any], output: Path | str) -> None:
    """Atomically write *manifest* as deterministic UTF-8 JSON to *output*."""
    validated_manifest = _validate_manifest(manifest)
    destination = _absolute_destination(output)
    _ensure_safe_output_parent(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="\n",
            delete=False,
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
        ) as temporary_file:
            temporary_path = Path(temporary_file.name)
            json.dump(validated_manifest, temporary_file, ensure_ascii=False, indent=2, sort_keys=True)
            temporary_file.write("\n")
            temporary_file.flush()
            os.fsync(temporary_file.fileno())
        os.replace(temporary_path, destination)
        temporary_path = None
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink(missing_ok=True)
            except OSError:
                pass


def create_manifest(root: Path | str, output: Path | str) -> Path:
    """Create a manifest for *root*, excluding an in-root *output* file itself."""
    resolved_root = _resolve_root(root)
    destination = _absolute_destination(output)
    _ensure_safe_output_parent(destination)
    try:
        destination.relative_to(resolved_root)
    except ValueError:
        excluded_paths: set[Path] = set()
    else:
        excluded_paths = {destination}
    write_manifest(_build_manifest(resolved_root, excluded_paths), destination)
    return destination


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    """Decode a JSON object while rejecting repeated keys."""
    decoded: dict[str, Any] = {}
    for key, value in pairs:
        if key in decoded:
            raise ValueError(f"duplicate JSON key: {key}")
        decoded[key] = value
    return decoded


def _load_manifest(manifest_path: Path) -> dict[str, Any]:
    """Load and validate a manifest file."""
    try:
        with manifest_path.open("r", encoding="utf-8") as manifest_file:
            decoded = json.load(manifest_file, object_pairs_hook=_reject_duplicate_keys)
    except json.JSONDecodeError as error:
        raise ValueError(f"manifest is not valid JSON: {error.msg}") from error
    return _validate_manifest(decoded)


def _manifest_path_inside_root(root: Path, manifest_path: Path) -> Path | None:
    """Return the manifest path when it is directly represented below *root*."""
    resolved_manifest = manifest_path.resolve()
    try:
        resolved_manifest.relative_to(root)
    except ValueError:
        return None
    return resolved_manifest


def verify_manifest(root: Path | str, manifest_path: Path | str) -> list[str]:
    """Compare *root* with a manifest and return sorted drift descriptions."""
    resolved_root = Path(root).resolve()
    if not resolved_root.is_dir():
        raise ValueError(f"root is not a directory: {resolved_root}")
    resolved_manifest_path = Path(manifest_path).resolve()
    expected = _load_manifest(resolved_manifest_path)
    manifest_inside_root = _manifest_path_inside_root(resolved_root, resolved_manifest_path)
    excluded_paths = {manifest_inside_root} if manifest_inside_root is not None else set()
    current = _build_manifest(resolved_root, excluded_paths)
    expected_by_path = {record["path"]: record for record in expected["files"]}
    current_by_path = {record["path"]: record for record in current["files"]}

    differences: list[str] = []
    for path in expected_by_path.keys() - current_by_path.keys():
        differences.append(f"missing:{path}")
    for path in current_by_path.keys() - expected_by_path.keys():
        differences.append(f"extra:{path}")
    for path in expected_by_path.keys() & current_by_path.keys():
        if expected_by_path[path] != current_by_path[path]:
            differences.append(f"changed:{path}")
    return sorted(differences)


def _build_parser() -> argparse.ArgumentParser:
    """Create the command-line parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    subcommands = parser.add_subparsers(dest="command", required=True)
    create_parser = subcommands.add_parser("create", help="create a manifest")
    create_parser.add_argument("root", metavar="ROOT")
    create_parser.add_argument("output", metavar="OUTPUT.json")
    verify_parser = subcommands.add_parser("verify", help="verify a manifest")
    verify_parser.add_argument("root", metavar="ROOT")
    verify_parser.add_argument("manifest", metavar="INPUT.json")
    return parser


def main(arguments: Sequence[str] | None = None) -> int:
    """Run the manifest command-line interface."""
    parser = _build_parser()
    parsed = parser.parse_args(arguments)
    try:
        if parsed.command == "create":
            output = create_manifest(parsed.root, parsed.output)
            print(output)
            return 0
        differences = verify_manifest(parsed.root, parsed.manifest)
    except (OSError, TypeError, ValueError) as error:
        parser.error(str(error))
    if differences:
        print("\n".join(differences))
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
