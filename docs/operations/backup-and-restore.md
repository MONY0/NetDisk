# NetDisk 备份与恢复运行手册

本手册仅适用于维护窗口或一致性文件系统快照。备份、凭据和恢复演练均在仓库外进行；恢复只能导入独立、可销毁的测试 MySQL 实例，绝不覆盖生产实例或生产文件。

## 前置条件

- 已使应用只读或使用文件系统快照；`mysqldump`、`mysql`、`rsync`、`python3`、`gzip`、`sha256sum` 可用。
- 当前用户能从实际仓库根目录运行命令；该目录可被 `netdisk` 读取。
- `/home/coder/NetDisk` 是当前代码写死的用户文件源路径，后续阶段将外部化；运行前必须确认部署主机确实使用它。
- Linux 上 `mysql_backup.py` 要求凭据文件所有者等于当前 UID。因此备份流程必须以 `netdisk` 运行，凭据文件由 `netdisk` 拥有且权限为 `600`；备份根目录由 `netdisk` 拥有且权限为 `700`。

不要把密码、备份、清单或证据提交到仓库，也不要让密码进入命令行、历史或输出。

## 管理员准备

管理员在受控终端运行下列完整流程。密码仅由受保护的交互 shell 读取；文件包含 `[client]`、`host`、`user`、`password` 四项。

```bash
set -euo pipefail
umask 077
sudo install -d -o netdisk -g netdisk -m 700 /etc/netdisk /var/backups/netdisk
sudo install -o netdisk -g netdisk -m 600 /dev/null /etc/netdisk/mysql-backup.cnf
sudo -u netdisk -- bash -s <<'NETDISK_CREDENTIALS'
set -euo pipefail
umask 077
set +o history
printf '%s\n' '[client]' 'host=localhost' 'user=netdisk_backup' > /etc/netdisk/mysql-backup.cnf
read -rsp 'MySQL backup password: ' NETDISK_DB_PASSWORD < /dev/tty
printf '\npassword=%s\n' "$NETDISK_DB_PASSWORD" >> /etc/netdisk/mysql-backup.cnf
unset NETDISK_DB_PASSWORD
NETDISK_CREDENTIALS
sudo stat -c '%U:%G %a %n' /etc/netdisk/mysql-backup.cnf /var/backups/netdisk
```

确认 `stat` 输出表明凭据文件和备份根目录均为 `netdisk:netdisk`，权限分别为 `600` 和 `700`，否则停止。

## 创建新的备份运行目录

从实际仓库根目录运行。每次都会创建新的私有 `run_dir`，不得复用或覆盖旧目录；将打印出的所有路径记录到本次证据中。

```bash
set -euo pipefail
umask 077
repo_root=$(pwd -P)
[[ -f "$repo_root/tools/baseline/mysql_backup.py" && ! -L "$repo_root/tools/baseline/mysql_backup.py" && -r "$repo_root/tools/baseline/mysql_backup.py" ]] || { printf '%s\n' 'start from the actual readable repository root' >&2; exit 1; }
[[ -f "$repo_root/tools/baseline/file_manifest.py" && ! -L "$repo_root/tools/baseline/file_manifest.py" && -r "$repo_root/tools/baseline/file_manifest.py" ]] || { printf '%s\n' 'manifest tool unavailable' >&2; exit 1; }
sudo -u netdisk -- bash -s -- "$repo_root" <<'NETDISK_BACKUP'
set -euo pipefail
umask 077
repo_root=$1
cd -- "$repo_root"
[[ -f tools/baseline/mysql_backup.py && ! -L tools/baseline/mysql_backup.py && -r tools/baseline/mysql_backup.py ]] || exit 1
[[ -f tools/baseline/file_manifest.py && ! -L tools/baseline/file_manifest.py && -r tools/baseline/file_manifest.py ]] || exit 1
backup_base=/var/backups/netdisk
[[ -d "$backup_base" && ! -L "$backup_base" && $(id -u) -eq $(stat -c '%u' "$backup_base") && $(stat -c '%a' "$backup_base") -eq 700 ]] || { printf '%s\n' 'unsafe backup base' >&2; exit 1; }
backup_base=$(readlink -f -- "$backup_base")
[[ -d "$backup_base" && ! -L "$backup_base" && $(id -u) -eq $(stat -c '%u' "$backup_base") && $(stat -c '%a' "$backup_base") -eq 700 ]] || { printf '%s\n' 'canonical backup base is unsafe' >&2; exit 1; }
run_dir=$(mktemp -d "$backup_base/run-XXXXXX")
chmod 700 "$run_dir"
mysql_dir="$run_dir/mysql"; files_dir="$run_dir/files"; manifest_path="$run_dir/files-before-optimization.json"
mkdir -m 700 -- "$mysql_dir" "$files_dir"
python3 tools/baseline/mysql_backup.py --defaults-file /etc/netdisk/mysql-backup.cnf --database NetDisk --output-dir "$mysql_dir" > "$run_dir/mysql-backup-paths.txt"
mapfile -t backup_paths < "$run_dir/mysql-backup-paths.txt"
[[ ${#backup_paths[@]} -eq 2 ]] || { printf '%s\n' 'backup did not return two paths' >&2; exit 1; }
archive_path=${backup_paths[0]}; checksum_path=${backup_paths[1]}
[[ "$archive_path" == "$mysql_dir"/*.sql.gz && "$checksum_path" == "${archive_path}.sha256" && -f "$archive_path" && -f "$checksum_path" && ! -L "$archive_path" && ! -L "$checksum_path" ]] || { printf '%s\n' 'invalid archive/checksum pair' >&2; exit 1; }
( cd -- "$mysql_dir"; sha256sum -c -- "$(basename -- "$checksum_path")" )
gzip -t -- "$archive_path"
python3 tools/baseline/file_manifest.py create /home/coder/NetDisk/ "$manifest_path"
rsync -aHAX --numeric-ids /home/coder/NetDisk/ "$files_dir/"
python3 tools/baseline/file_manifest.py verify "$files_dir/" "$manifest_path"
printf 'run_dir=%s\narchive=%s\nchecksum=%s\nmanifest=%s\nfiles=%s\n' "$run_dir" "$archive_path" "$checksum_path" "$manifest_path" "$files_dir"
NETDISK_BACKUP
```

有效数据库备份必须同时有 `.sql.gz` 与相邻 `.sql.gz.sha256`，并通过 SHA-256 和 gzip 检查。检查失败会因严格 shell 设置而终止，且工具不会产生明文 SQL 暂存文件。

## 仅限隔离环境的恢复演练

转储使用 `--databases NetDisk`，含创建数据库和 `USE` 语义；不能仅改数据库名在生产实例内恢复。新会话从记录的 `run_dir` 唯一派生数据库和文件备份，禁止混搭其他运行目录、通配符或“最新文件”。

```bash
set -euo pipefail
umask 077
repo_root=$(pwd -P)
[[ -f "$repo_root/tools/baseline/file_manifest.py" && ! -L "$repo_root/tools/baseline/file_manifest.py" && -r "$repo_root/tools/baseline/file_manifest.py" ]] || { printf '%s\n' 'start from the actual readable repository root' >&2; exit 1; }
sudo -u netdisk -- bash -s -- "$repo_root" <<'NETDISK_RESTORE'
set -euo pipefail
umask 077
repo_root=$1
cd -- "$repo_root"
[[ -f tools/baseline/file_manifest.py && ! -L tools/baseline/file_manifest.py && -r tools/baseline/file_manifest.py ]] || exit 1
read -r -p 'Recorded absolute run directory: ' run_dir < /dev/tty
backup_base=/var/backups/netdisk
[[ -n "$run_dir" && "$run_dir" = /* && "$run_dir" != *'/..'* && "$run_dir" != '..' && "$run_dir" != *'/../'* && -d "$run_dir" ]] || { printf '%s\n' 'run directory must be an existing absolute path without dot-dot components' >&2; exit 1; }
path_probe=$run_dir
while [[ "$path_probe" != / ]]; do
  [[ ! -L "$path_probe" ]] || { printf '%s\n' 'run directory path chain contains a symbolic link' >&2; exit 1; }
  path_probe=$(dirname -- "$path_probe")
done
[[ ! -L / ]] || { printf '%s\n' 'root path is linked' >&2; exit 1; }
backup_base=$(readlink -f -- "$backup_base")
run_dir=$(readlink -f -- "$run_dir")
[[ -d "$backup_base" && ! -L "$backup_base" && $(id -u) -eq $(stat -c '%u' "$backup_base") && $(stat -c '%a' "$backup_base") -eq 700 ]] || { printf '%s\n' 'canonical backup base is unsafe' >&2; exit 1; }
[[ $(dirname -- "$run_dir") == "$backup_base" && $(basename -- "$run_dir") == run-* && -d "$run_dir" && ! -L "$run_dir" && $(id -u) -eq $(stat -c '%u' "$run_dir") && $(stat -c '%a' "$run_dir") -eq 700 ]] || { printf '%s\n' 'invalid private netdisk run directory' >&2; exit 1; }
mysql_dir="$run_dir/mysql"; manifest_path="$run_dir/files-before-optimization.json"; files_dir="$run_dir/files"
[[ -d "$mysql_dir" && ! -L "$mysql_dir" && -f "$manifest_path" && ! -L "$manifest_path" && -d "$files_dir" && ! -L "$files_dir" ]] || { printf '%s\n' 'run children missing or linked' >&2; exit 1; }
mapfile -t archives < <(find "$mysql_dir" -maxdepth 1 -type f -name '*.sql.gz' -links 1 -print)
[[ ${#archives[@]} -eq 1 ]] || { printf '%s\n' 'run must contain exactly one regular archive' >&2; exit 1; }
archive_path=${archives[0]}; checksum_path="${archive_path}.sha256"
[[ -f "$checksum_path" && ! -L "$checksum_path" ]] || { printf '%s\n' 'adjacent checksum missing' >&2; exit 1; }
( cd -- "$mysql_dir"; sha256sum -c -- "$(basename -- "$checksum_path")" )
gzip -t -- "$archive_path"
printf '%s\n' 'Manual gate: verify mysql-restore-test.cnf targets only a disposable isolated test MySQL host.'
read -r -p 'Type ISOLATED-TEST-INSTANCE to permit streaming import: ' confirmation < /dev/tty
[[ "$confirmation" == ISOLATED-TEST-INSTANCE ]] || { printf '%s\n' 'import stopped' >&2; exit 1; }
gzip -dc -- "$archive_path" | mysql --defaults-file=/etc/netdisk/mysql-restore-test.cnf
restore_files_dir=$(mktemp -d /var/tmp/netdisk-restore-test-XXXXXX)
chmod 700 "$restore_files_dir"
rsync -aHAX --numeric-ids "$files_dir/" "$restore_files_dir/"
python3 tools/baseline/file_manifest.py verify "$restore_files_dir/" "$manifest_path"
printf 'restored_files=%s\n' "$restore_files_dir"
NETDISK_RESTORE
```

`pipefail` 覆盖流式 `gzip -dc | mysql`，不创建明文 SQL。恢复文件目标是新的私有目录。导入后检查行数、结构、索引、触发器、视图并运行应用冒烟测试；证据必须脱敏。

## 异常退出与残留物

归档单独出现不表示可恢复；只有校验文件而没有归档，即使运行目录仍在，也是不完整备份。`.netdisk-backup-*` 暂存目录和所有孤立残留均不得恢复，且不得自动删除。确认没有运行中的进程后才可检查；下列流程只检查和记录，绝不删除或移动。

```bash
set -euo pipefail
umask 077
if pgrep -af 'mysql_backup\.py|mysqldump'; then
  printf '%s\n' 'backup process is still running; residue inspection stopped' >&2
  exit 1
fi
find /var/backups/netdisk -maxdepth 3 \( -name '*.sql.gz' -o -name '*.sql.gz.sha256' -o -name '.netdisk-backup-*' -o -name '.NetDisk-staging-*' \) -ls
```

受权人员可在审计记录后按本地保留规则手工隔离可疑残留，不能盲目清理。

## Phase 0 状态

部署主机的备份、恢复和演练检查点尚未在本地执行；本手册不是已通过生产验证的声明。
