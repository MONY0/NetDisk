# NetDisk Phase 0 Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a safe version-control baseline, authoritative project map, reproducible backup tooling, and a repeatable smoke-test record before changing NetDisk business logic.

**Architecture:** Phase 0 adds repository hygiene, documentation, and standalone Python baseline tools without changing the Qt client or Linux server behavior. File manifests are deterministic JSON records containing size, MD5, and SHA-256; MySQL backups are produced through `mysqldump` using a protected defaults file and accompanied by a checksum. Live database and file backups remain an explicit deployment-host checkpoint because the current workspace has no connection to the Linux runtime.

**Tech Stack:** Git 2.52+, Python 3.12 standard library and `unittest`, qmake, MySQL command-line tools, Markdown.

---

## Scope and prerequisites

This plan implements only phase 0 of `docs/superpowers/specs/2026-08-11-netdisk-optimization-design.md`. It does not modify authentication, the wire protocol, SQL behavior, networking, or transfer logic.

The requested worktree cannot be created yet because `D:\Qt -dm\NetDisk\.git` is an empty directory rather than a valid repository. Task 1 initializes the repository in place after installing ignore rules. Future phases must use a dedicated worktree.

Authoritative source roots:

- Client: `D:\Qt -dm\NetDisk`
- Linux server: `D:\Qt -dm\NetDisk\NetDisk_server\NetDisk`
- Legacy server snapshot: `D:\Qt -dm\NetDisk\src` and `D:\Qt -dm\NetDisk\include`

## Planned file structure

- Create `.gitignore`: exclude generated files, runtime data, credentials, backups, and local IDE state.
- Create `README.md`: identify the two supported build roots and link operational documents.
- Create `docs/project-layout.md`: record authoritative and legacy source boundaries.
- Create `docs/build-and-run.md`: record Windows client and Linux server build commands and dependencies.
- Create `docs/testing/smoke-test.md`: define the baseline evidence matrix.
- Create `docs/operations/backup-and-restore.md`: provide safe backup, verification, and restore commands.
- Create `tools/baseline/file_manifest.py`: create and verify deterministic file manifests.
- Create `tools/baseline/mysql_backup.py`: create compressed, checksummed MySQL dumps without embedding credentials.
- Create `tests/baseline/test_file_manifest.py`: test file manifest creation and drift detection.
- Create `tests/baseline/test_mysql_backup.py`: test dump command construction and failure cleanup.

### Task 1: Establish safe Git baseline

**Files:**
- Create: `.gitignore`

- [ ] **Step 1: Verify the existing `.git` directory is empty**

Run:

```powershell
(Get-ChildItem -Force .git -Recurse | Measure-Object).Count
```

Expected: `0`. If it is not zero, stop and inspect it instead of reinitializing.

- [ ] **Step 2: Create the ignore rules before staging anything**

Create `.gitignore` with:

```gitignore
# Qt Creator and local kit state
.qtcreator/
*.pro.user
*.pro.user.*

# Build outputs
bulid-*/
build-*/
**/build-*/
debug/
release/
*.o
*.obj
*.exe
*.dll
*.so
*.a
*.lib
Makefile
Makefile.*
object_script.*
moc_*.cpp
qrc_*.cpp
ui_*.h

# Runtime data and secrets
config.ini
*.db
*.db-journal
*.sqlite
*.sqlite3
.env
.env.*
*.pem
*.key
*.p12
*.pfx
NetDisk_server/[0-9]*/

# Backup and generated evidence
backups/
artifacts/
__pycache__/
*.py[cod]

# OS/editor files
.DS_Store
Thumbs.db
*.swp
```

- [ ] **Step 3: Initialize Git without changing source files**

Run:

```powershell
git init
git status --short --branch
```

Expected: repository initializes successfully and ignored build/runtime files are absent from the untracked list.

- [ ] **Step 4: Prove sensitive and generated paths are ignored**

Run:

```powershell
git check-ignore -v bulid-release/release/NetDisk.exe
git check-ignore -v bulid-release/release/config.ini
git check-ignore -v bulid-release/release/database/1.db
git check-ignore -v NetDisk_server/1/0335dff574ca52fb88c1c7adc6587e1b
git check-ignore -v NetDisk_server/build-NetDisk-Desktop_Qt_5_9_9_GCC_64bit-Debug/NetDisk
```

Expected: every command prints the matching `.gitignore` rule.

- [ ] **Step 5: Stage and audit the initial source baseline**

Run:

```powershell
git add --all
git diff --cached --name-only
git diff --cached --name-only | Select-String -Pattern '(^|/)(config\.ini|.*\.db|NetDisk_server/1/|bulid-release/|build-NetDisk-)'
```

Expected: the final command produces no matches. If it prints a path, unstage everything with `git restore --staged .`, correct `.gitignore`, and repeat.

- [ ] **Step 6: Commit the baseline**

Run:

```powershell
git commit -m "chore: establish NetDisk source baseline"
```

Expected: commit succeeds. If Git reports missing user identity, stop and ask the user to configure their identity; do not invent one.

### Task 2: Document authoritative project boundaries

**Files:**
- Create: `README.md`
- Create: `docs/project-layout.md`
- Create: `docs/build-and-run.md`

- [ ] **Step 1: Write the project entry-point documentation**

`README.md` must identify `NetDisk.pro` as the Qt client project, `NetDisk_server/NetDisk/NetDisk.pro` as the Linux server project, and link the design, project-layout, build, smoke-test, and backup documents. It must explicitly state that root `src/` and `include/` are legacy snapshots and are not implementation targets.

- [ ] **Step 2: Record the project map**

`docs/project-layout.md` must map these flows:

```text
Client: main.cpp -> CKernel -> TcpClientMediator -> TcpClient
Server: src/main.cpp -> TcpKernel -> Block_Epoll_Net -> CLogic -> CMysql/filesystem
```

It must also record that `NetDisk_server/NetDisk/main.cpp` is an unused Hello World entry and that `NetDisk_server/NetDisk/src/main.cpp` is the server build entry.

- [ ] **Step 3: Record exact build prerequisites and commands**

`docs/build-and-run.md` must include:

```powershell
# Windows client, from a Qt developer shell
New-Item -ItemType Directory -Force ..\NetDisk-build-client
Set-Location ..\NetDisk-build-client
qmake "D:\Qt -dm\NetDisk\NetDisk.pro"
nmake
```

```bash
# Linux server
sudo apt-get install build-essential qt5-qmake default-libmysqlclient-dev
mkdir -p ../NetDisk-build-server
cd ../NetDisk-build-server
qmake ../NetDisk/NetDisk.pro
make -j"$(nproc)"
./NetDisk 8000
```

The document must label package names as Debian/Ubuntu examples and must not claim Qt 6 compatibility until a build proves it.

- [ ] **Step 4: Verify the documentation contains the required boundaries**

Run:

```powershell
rg -n "NetDisk\.pro|NetDisk_server/NetDisk|legacy|旧服务端|src/main\.cpp|qmake" README.md docs/project-layout.md docs/build-and-run.md
```

Expected: each concept appears in the appropriate document.

- [ ] **Step 5: Commit the documentation**

```powershell
git add README.md docs/project-layout.md docs/build-and-run.md
git commit -m "docs: define NetDisk project boundaries and builds"
```

### Task 3: Add deterministic file manifest tooling

**Files:**
- Create: `tests/baseline/test_file_manifest.py`
- Create: `tools/baseline/file_manifest.py`

- [ ] **Step 1: Write failing manifest tests**

The tests must create a temporary tree, assert stable relative POSIX paths, sizes, MD5 and SHA-256 values, then alter a file and assert verification reports `changed:a.txt`. They must import `build_manifest`, `write_manifest`, and `verify_manifest` from `tools.baseline.file_manifest`.

Create `tests/baseline/test_file_manifest.py` with:

```python
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.baseline.file_manifest import (
    build_manifest,
    verify_manifest,
    write_manifest,
)


class FileManifestTests(unittest.TestCase):
    def test_build_manifest_records_hashes_and_sorted_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "z.txt").write_bytes(b"z")
            (root / "nested").mkdir()
            (root / "nested" / "a.txt").write_bytes(b"abc")

            manifest = build_manifest(root)

            self.assertEqual(
                [item["path"] for item in manifest["files"]],
                ["nested/a.txt", "z.txt"],
            )
            first = manifest["files"][0]
            self.assertEqual(first["size"], 3)
            self.assertEqual(first["md5"], hashlib.md5(b"abc").hexdigest())
            self.assertEqual(first["sha256"], hashlib.sha256(b"abc").hexdigest())

    def test_verify_manifest_detects_changed_missing_and_extra_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "files"
            root.mkdir()
            (root / "a.txt").write_bytes(b"a")
            (root / "missing.txt").write_bytes(b"missing")
            manifest_path = Path(directory) / "manifest.json"
            write_manifest(build_manifest(root), manifest_path)

            (root / "a.txt").write_bytes(b"changed")
            (root / "missing.txt").unlink()
            (root / "extra.txt").write_bytes(b"extra")

            self.assertEqual(
                verify_manifest(root, manifest_path),
                ["changed:a.txt", "extra:extra.txt", "missing:missing.txt"],
            )

    def test_write_manifest_is_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "files"
            root.mkdir()
            (root / "a.txt").write_bytes(b"a")
            first = Path(directory) / "first.json"
            second = Path(directory) / "second.json"
            manifest = build_manifest(root)

            write_manifest(manifest, first)
            write_manifest(manifest, second)

            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(json.loads(first.read_text(encoding="utf-8"))["version"], 1)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the tests and verify the module is missing**

Run:

```powershell
python -m unittest discover -s tests/baseline -p "test_file_manifest.py" -v
```

Expected: FAIL because `tools.baseline.file_manifest` does not exist.

- [ ] **Step 3: Implement the manifest tool**

Create `tools/baseline/file_manifest.py` with:

```python
from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path
from typing import Any

CHUNK_SIZE = 1024 * 1024


def _hash_file(path: Path) -> tuple[str, str]:
    md5 = hashlib.md5(usedforsecurity=False)
    sha256 = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(CHUNK_SIZE):
            md5.update(chunk)
            sha256.update(chunk)
    return md5.hexdigest(), sha256.hexdigest()


def build_manifest(root: Path | str) -> dict[str, Any]:
    root_path = Path(root).resolve()
    if not root_path.is_dir():
        raise ValueError(f"manifest root is not a directory: {root_path}")

    files: list[dict[str, Any]] = []
    paths = sorted(root_path.rglob("*"), key=lambda item: item.as_posix())
    for path in paths:
        if path.is_symlink() or not path.is_file():
            continue
        md5, sha256 = _hash_file(path)
        files.append(
            {
                "path": path.relative_to(root_path).as_posix(),
                "size": path.stat().st_size,
                "md5": md5,
                "sha256": sha256,
            }
        )
    return {"version": 1, "files": files}


def write_manifest(manifest: dict[str, Any], output: Path | str) -> None:
    output_path = Path(output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(
        manifest,
        ensure_ascii=False,
        indent=2,
        sort_keys=True,
    ) + "\n"
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            newline="\n",
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary:
            temporary.write(payload)
            temporary.flush()
            os.fsync(temporary.fileno())
            temporary_name = temporary.name
        os.replace(temporary_name, output_path)
        temporary_name = None
    finally:
        if temporary_name:
            Path(temporary_name).unlink(missing_ok=True)


def verify_manifest(root: Path | str, manifest_path: Path | str) -> list[str]:
    expected = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
    if expected.get("version") != 1 or not isinstance(expected.get("files"), list):
        raise ValueError("unsupported or malformed manifest")

    current = build_manifest(root)
    expected_files = {item["path"]: item for item in expected["files"]}
    current_files = {item["path"]: item for item in current["files"]}
    differences: list[str] = []

    for path in sorted(expected_files.keys() - current_files.keys()):
        differences.append(f"missing:{path}")
    for path in sorted(current_files.keys() - expected_files.keys()):
        differences.append(f"extra:{path}")
    for path in sorted(expected_files.keys() & current_files.keys()):
        if expected_files[path] != current_files[path]:
            differences.append(f"changed:{path}")
    return sorted(differences)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create or verify a NetDisk file manifest")
    subparsers = parser.add_subparsers(dest="command", required=True)
    create = subparsers.add_parser("create")
    create.add_argument("root", type=Path)
    create.add_argument("output", type=Path)
    verify = subparsers.add_parser("verify")
    verify.add_argument("root", type=Path)
    verify.add_argument("manifest", type=Path)
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    if args.command == "create":
        write_manifest(build_manifest(args.root), args.output)
        print(args.output.resolve())
        return 0

    differences = verify_manifest(args.root, args.manifest)
    if differences:
        for difference in differences:
            print(difference)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 4: Run the focused tests**

Run:

```powershell
python -m unittest discover -s tests/baseline -p "test_file_manifest.py" -v
```

Expected: 3 tests pass.

- [ ] **Step 5: Exercise the CLI against a temporary directory**

Run:

```powershell
$root = Join-Path $env:TEMP 'netdisk-manifest-smoke'
$out = Join-Path $env:TEMP 'netdisk-manifest-smoke.json'
New-Item -ItemType Directory -Force $root | Out-Null
Set-Content -Encoding utf8 (Join-Path $root 'sample.txt') 'netdisk'
python tools/baseline/file_manifest.py create $root $out
python tools/baseline/file_manifest.py verify $root $out
```

Expected: both Python commands exit 0 and verification prints `OK`.

- [ ] **Step 6: Commit the manifest tool**

```powershell
git add tools/baseline/file_manifest.py tests/baseline/test_file_manifest.py
git commit -m "test: add file backup manifest verification"
```

### Task 4: Add safe MySQL backup tooling

**Files:**
- Create: `tests/baseline/test_mysql_backup.py`
- Create: `tools/baseline/mysql_backup.py`

- [ ] **Step 1: Write failing backup tests**

Create `tests/baseline/test_mysql_backup.py` with:

```python
import subprocess
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

from tools.baseline.mysql_backup import build_dump_command, create_backup


class MysqlBackupTests(unittest.TestCase):
    def test_dump_command_places_defaults_file_first(self):
        command = build_dump_command(Path("mysql.cnf"), "NetDisk")
        self.assertEqual(
            command,
            [
                "mysqldump",
                "--defaults-extra-file=mysql.cnf",
                "--single-transaction",
                "--routines",
                "--triggers",
                "--events",
                "--hex-blob",
                "--databases",
                "NetDisk",
            ],
        )

    def test_backup_rejects_missing_defaults_file(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileNotFoundError):
                create_backup(
                    Path(directory) / "missing.cnf",
                    "NetDisk",
                    Path(directory) / "out",
                )

    def test_failed_dump_leaves_no_final_archive(self):
        def failing_runner(*args, **kwargs):
            return subprocess.CompletedProcess(args[0], 2, stderr=b"dump failed")

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            defaults_file = root / "mysql.cnf"
            defaults_file.write_text("[client]\nuser=test\n", encoding="utf-8")
            output_dir = root / "out"
            with self.assertRaises(RuntimeError):
                create_backup(
                    defaults_file,
                    "NetDisk",
                    output_dir,
                    now=datetime(2026, 8, 11, 12, 30, 0),
                    runner=failing_runner,
                )
            self.assertEqual(list(output_dir.glob("*")), [])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the tests and verify they fail**

```powershell
python -m unittest discover -s tests/baseline -p "test_mysql_backup.py" -v
```

Expected: FAIL because `tools.baseline.mysql_backup` does not exist.

- [ ] **Step 3: Implement the backup tool**

Create `tools/baseline/mysql_backup.py` with:

```python
from __future__ import annotations

import argparse
import gzip
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path
from typing import Protocol


class Runner(Protocol):
    def __call__(
        self,
        *args: object,
        **kwargs: object,
    ) -> subprocess.CompletedProcess[bytes]:
        pass


def build_dump_command(
    defaults_file: Path,
    database: str,
    executable: str = "mysqldump",
) -> list[str]:
    return [
        executable,
        f"--defaults-extra-file={defaults_file}",
        "--single-transaction",
        "--routines",
        "--triggers",
        "--events",
        "--hex-blob",
        "--databases",
        database,
    ]


def _warn_if_permissions_are_open(path: Path) -> None:
    if os.name == "posix" and stat.S_IMODE(path.stat().st_mode) & 0o077:
        print(
            f"warning: {path} is readable or writable by group/other; use chmod 600",
            file=sys.stderr,
        )


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def create_backup(
    defaults_file: Path,
    database: str,
    output_dir: Path,
    *,
    now: datetime | None = None,
    runner: Runner = subprocess.run,
) -> Path:
    defaults_file = defaults_file.resolve()
    if not defaults_file.is_file():
        raise FileNotFoundError(f"MySQL defaults file not found: {defaults_file}")
    if not database or not database.replace("_", "").isalnum():
        raise ValueError("database name may contain only letters, digits, and underscores")
    _warn_if_permissions_are_open(defaults_file)

    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    timestamp = (now or datetime.now()).strftime("%Y%m%d-%H%M%S")
    archive = output_dir / f"{database}-{timestamp}.sql.gz"
    checksum = archive.with_suffix(archive.suffix + ".sha256")
    raw_name: str | None = None
    gzip_name: str | None = None

    try:
        with tempfile.NamedTemporaryFile(
            "wb", dir=output_dir, prefix=".netdisk-dump-", suffix=".sql", delete=False
        ) as raw:
            raw_name = raw.name
            result = runner(
                build_dump_command(defaults_file, database),
                stdout=raw,
                stderr=subprocess.PIPE,
                check=False,
            )
            raw.flush()
            os.fsync(raw.fileno())
        if result.returncode != 0:
            message = result.stderr.decode("utf-8", errors="replace").strip()
            raise RuntimeError(f"mysqldump failed with exit {result.returncode}: {message}")

        with tempfile.NamedTemporaryFile(
            "wb", dir=output_dir, prefix=".netdisk-dump-", suffix=".sql.gz", delete=False
        ) as compressed_file:
            gzip_name = compressed_file.name
            with Path(raw_name).open("rb") as source, gzip.GzipFile(
                filename="",
                mode="wb",
                fileobj=compressed_file,
                mtime=0,
            ) as compressed:
                shutil.copyfileobj(source, compressed, length=1024 * 1024)
            compressed_file.flush()
            os.fsync(compressed_file.fileno())

        os.replace(gzip_name, archive)
        gzip_name = None
        digest = _sha256(archive)
        checksum.write_text(f"{digest}  {archive.name}\n", encoding="ascii", newline="\n")
        return archive
    except Exception:
        archive.unlink(missing_ok=True)
        checksum.unlink(missing_ok=True)
        raise
    finally:
        if raw_name:
            Path(raw_name).unlink(missing_ok=True)
        if gzip_name:
            Path(gzip_name).unlink(missing_ok=True)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Create a compressed NetDisk MySQL backup. The defaults file must contain "
            "a [client] section with host, user, and password and should be chmod 600."
        )
    )
    parser.add_argument("--defaults-file", required=True, type=Path)
    parser.add_argument("--database", default="NetDisk")
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    try:
        archive = create_backup(args.defaults_file, args.database, args.output_dir)
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(archive)
    print(archive.with_suffix(archive.suffix + ".sha256"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

The deployment defaults file documented in Task 5 is:

```ini
[client]
host=localhost
user=netdisk_backup
password=replace-at-deployment
```

- [ ] **Step 4: Run all baseline unit tests**

```powershell
python -m unittest discover -s tests/baseline -p "test_*.py" -v
```

Expected: 6 tests pass.

- [ ] **Step 5: Commit the MySQL backup tool**

```powershell
git add tools/baseline/mysql_backup.py tests/baseline/test_mysql_backup.py
git commit -m "feat: add safe MySQL baseline backup tool"
```

### Task 5: Add backup, restore, and smoke-test runbooks

**Files:**
- Create: `docs/operations/backup-and-restore.md`
- Create: `docs/testing/smoke-test.md`

- [ ] **Step 1: Write the backup procedure**

The procedure must require a maintenance window or read-only state and use paths outside the repository:

```bash
python3 tools/baseline/mysql_backup.py \
  --defaults-file /etc/netdisk/mysql-backup.cnf \
  --database NetDisk \
  --output-dir /var/backups/netdisk/mysql

python3 tools/baseline/file_manifest.py create \
  /home/coder/NetDisk \
  /var/backups/netdisk/files-before-optimization.json

rsync -aHAX --numeric-ids /home/coder/NetDisk/ \
  /var/backups/netdisk/files/

python3 tools/baseline/file_manifest.py verify \
  /var/backups/netdisk/files \
  /var/backups/netdisk/files-before-optimization.json
```

It must explain that the checked-in `/home/coder/NetDisk` path reflects the current code and will be externalized in a later phase.

- [ ] **Step 2: Write restore verification instructions**

Restore must target a disposable database and temporary directory first. The document must include `sha256sum -c`, `gzip -t`, a MySQL import into a test instance, row-count checks, file-manifest verification, and a warning not to overwrite live data during the drill.

- [ ] **Step 3: Write the smoke-test evidence table**

`docs/testing/smoke-test.md` must have columns:

```text
Case | Preconditions | Steps | Expected result | Actual result | Evidence | Status
```

Cases must cover registration, login, listing existing files, upload, quick upload, download, folder transfer, folder creation, share, share retrieval, delete, pause, resume, disconnect, client crash, server restart, invalid packet, and unauthorized resource access. Every completed transfer must record size and MD5.

- [ ] **Step 4: Verify required safety language and test cases**

```powershell
rg -n "maintenance|read-only|disposable|temporary|sha256sum|gzip -t|MD5|server restart|unauthorized" docs/operations/backup-and-restore.md docs/testing/smoke-test.md
```

Expected: all concepts are found.

- [ ] **Step 5: Commit the runbooks**

```powershell
git add docs/operations/backup-and-restore.md docs/testing/smoke-test.md
git commit -m "docs: add NetDisk backup and smoke-test runbooks"
```

### Task 6: Verify the local phase-0 deliverables

**Files:**
- Modify only if verification finds a defect in a phase-0 file.

- [ ] **Step 1: Run all Python baseline tests**

```powershell
python -m unittest discover -s tests/baseline -p "test_*.py" -v
```

Expected: 6 tests pass and 0 fail.

- [ ] **Step 2: Check Python syntax**

```powershell
python -m compileall -q tools/baseline tests/baseline
```

Expected: exit code 0.

- [ ] **Step 3: Audit tracked files for runtime data and credentials**

```powershell
$tracked = git ls-files
$blocked = $tracked | Select-String -Pattern '(^|/)(config\.ini|.*\.db|NetDisk_server/[0-9]+/|bulid-release/|build-NetDisk-|.*\.pem|.*\.key)$'
$blocked
if ($blocked) { exit 1 }
```

Expected: no output and exit code 0.

- [ ] **Step 4: Verify documentation links and required sections**

```powershell
rg -n "project-layout|build-and-run|smoke-test|backup-and-restore|optimization-design" README.md
rg -n "阶段 0|稳定性与数据一致性|安全与配置|协议 V2|性能优化" docs/superpowers/specs/2026-08-11-netdisk-optimization-design.md
```

Expected: all links and stage headings are found.

- [ ] **Step 5: Record local limitations honestly**

Do not mark phase 0 operationally complete until the following have been performed on the Linux deployment host:

- MySQL backup and restore drill
- user-file backup and manifest verification
- Linux server build
- baseline smoke-test evidence capture

The local workspace can complete tooling and documentation, but it cannot prove the live backups or Linux build without access to that host.

- [ ] **Step 6: Confirm clean repository state**

```powershell
git status --short --branch
git log --oneline -5
```

Expected: no uncommitted phase-0 changes and the baseline commits are visible.

## Completion checkpoint

After Tasks 1–6, stop before changing client or server behavior. Report separately:

- locally verified deliverables;
- deployment-host checks still required;
- any build or backup failures with exact commands and output.

Phase 1 stability work may start only after the real database and file backups have been restored successfully in a disposable environment.
