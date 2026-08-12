# NetDisk

NetDisk 是一个 C++/Qt 网盘项目，包含 Windows 客户端和 Linux 服务端。当前仓库的权威实现边界如下：

- 客户端：仓库根目录，qmake 工程为 `NetDisk.pro`，入口为 `main.cpp`。
- Linux 服务端：`NetDisk_server/NetDisk`，qmake 工程为 `NetDisk_server/NetDisk/NetDisk.pro`，实际入口为 `NetDisk_server/NetDisk/src/main.cpp`。

> 警告：仓库根目录的 `src/` 与 `include/` 是旧服务端快照，不是当前服务端实现目标。不要向它们提交业务逻辑、服务端修复或功能变更；后续服务端改动只应位于 `NetDisk_server/NetDisk` 下。

## 主要功能

- 用户注册与登录。
- 文件和文件夹的上传、下载与断点续传。
- 服务端文件元数据与用户关系管理。
- 文件/文件夹共享、查看我的分享以及通过分享链接保存资源。
- 客户端传输任务的本地 SQLite 持久化。

## 文档

- [项目布局与权威边界](docs/project-layout.md)
- [构建与运行](docs/build-and-run.md)
- [冒烟测试](docs/testing/smoke-test.md)
- [备份与恢复](docs/operations/backup-and-restore.md)
- [优化设计](docs/superpowers/specs/2026-08-11-netdisk-optimization-design.md)
- [Phase 0 基线计划](docs/superpowers/plans/2026-08-11-netdisk-phase-0-baseline.md)

Phase 0 尚未完成干净环境构建或测试验证；本文档不表示任何构建或测试已经通过。
