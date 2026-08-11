# 构建与运行

本页记录当前工程文件所表达的构建入口。Phase 0 尚未在干净环境完成构建验证，以下命令是待验证的基线步骤，不能据此推断构建已经通过。当前找到的 Windows `qmake` 为 Qt 6.11 MSVC；项目对 Qt 6 的兼容性尚未验证，也不应据此宣称支持 Qt 6。

## 前置条件

### Windows 客户端

- 在 Qt 开发者命令行中运行（需能使用对应 MSVC 的 `qmake` 与 `nmake`）。
- Qt 模块：Core、Gui、Widgets、Sql。
- Qt SQLite 驱动（客户端本地传输任务使用 `QSQLITE`）。
- Winsock（Windows 网络实现依赖）。

### Linux 服务端

- C++11 编译器与构建工具。
- pthread。
- MySQL 客户端头文件与库。
- Linux epoll。
- 下列包名是 Debian/Ubuntu 示例；其他发行版应使用其对应的软件包名称。

## Windows 客户端构建

在 **Qt 开发者命令行** 中，从仓库根目录执行。下列命令是本工作区 `D:\Qt -dm\NetDisk` 的固定示例，按基线要求保留：

```powershell
New-Item -ItemType Directory -Force ..\NetDisk-build-client
Set-Location ..\NetDisk-build-client
qmake "D:\Qt -dm\NetDisk\NetDisk.pro"
nmake
```

对于克隆在其他位置的仓库，可在仓库根目录执行下列可移植命令；`$repoRoot` 在第一行取当前仓库根目录，进入同级构建目录后仍以该绝对路径引用工程：

```powershell
$repoRoot = (Resolve-Path .).Path
$buildRoot = Join-Path (Split-Path $repoRoot -Parent) 'NetDisk-build-client'
New-Item -ItemType Directory -Force $buildRoot
Set-Location $buildRoot
qmake (Join-Path $repoRoot 'NetDisk.pro')
nmake
```

客户端连接端点由可执行文件相邻的 `config.ini` 提供。首次启动时，`CKernel::loadIniFile()` 会创建该文件，并写入如下内容：

```ini
[net]
ip=192.168.79.132
port=8000
```

其中 `192.168.79.132` 是源码中的开发环境默认 IP，通常必须改为实际服务端端点；`port` 默认是 `8000`。编辑 `config.ini` 后重启客户端，使其重新读取配置。客户端还会在可执行文件相邻位置创建或使用本地 `database/` 目录保存 SQLite 传输任务状态。

## Linux 服务端构建与运行

以下命令在 Linux 上、以服务端源码目录 `NetDisk_server/NetDisk` 为当前目录执行。软件包名为 Debian/Ubuntu 示例：

```bash
sudo apt-get install build-essential qt5-qmake default-libmysqlclient-dev
mkdir -p ../NetDisk-build-server
cd ../NetDisk-build-server
qmake ../NetDisk/NetDisk.pro
make -j"$(nproc)"
./NetDisk 8000
```

服务端默认端口为 `8000`；传入第一个命令行参数可覆盖它，例如 `./NetDisk 8000`。服务端需要可用的 MySQL 服务以及适当的文件系统存储位置和权限。

> 安全警告：当前源码在 `NetDisk_server/NetDisk/include/packdef.h` 硬编码 MySQL `root/123456`，并在 `NetDisk_server/NetDisk/src/clogic.cpp` 硬编码文件存储根目录 `/home/coder/NetDisk/`。在后续阶段完成外置化之前，只能使用隔离的测试数据库和测试存储目录，禁止将其暴露或部署为生产服务；当前并不存在可用于覆盖这些值的既有服务端配置文件。

请勿使用 `NetDisk_server/NetDisk/main.cpp` 的 Hello World 示例作为服务端入口，也不要在仓库根目录 `src/` 或 `include/` 的旧服务端快照中进行服务端业务修改。真实服务端工程和入口分别是 `NetDisk_server/NetDisk/NetDisk.pro` 与 `NetDisk_server/NetDisk/src/main.cpp`。
