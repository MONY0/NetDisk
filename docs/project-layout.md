# 项目布局与权威边界

## 权威目录

| 范围 | 权威位置 | 工程与入口 | 维护规则 |
| --- | --- | --- | --- |
| Windows 客户端 | 仓库根目录 | `NetDisk.pro`；`main.cpp` | 客户端界面、协议处理和本地传输状态在此维护。 |
| Linux 服务端 | `NetDisk_server/NetDisk` | `NetDisk_server/NetDisk/NetDisk.pro`；`NetDisk_server/NetDisk/src/main.cpp` | 所有未来服务端业务变更只允许在此目录下进行。 |
| 旧服务端快照 | 根目录 `src/`、`include/` | 不属于当前服务端工程 | 不得作为实现目标，也不得向其中提交业务变更。 |

`NetDisk_server/NetDisk/main.cpp` 仅是输出 “Hello World!” 的未使用示例。服务端的 `.pro` 文件引用的是 `src/main.cpp`，因此真实服务端入口不是这个示例文件。

## 启动与调用链

客户端的精确调用链：

```text
main.cpp -> CKernel -> TcpClientMediator -> TcpClient
```

`main.cpp` 创建 Qt 应用并取得 `CKernel` 单例；`CKernel` 负责协议分发、界面协作、客户端配置和本地任务状态；`TcpClientMediator` 作为网络中介协调收发；`TcpClient` 使用 Winsock 完成客户端连接与数据传输。

Linux 服务端的精确调用链：

```text
src/main.cpp -> TcpKernel -> Block_Epoll_Net -> CLogic -> CMysql/filesystem
```

`src/main.cpp` 读取可选端口参数并启动 `TcpKernel`；`TcpKernel` 初始化 MySQL、网络层及协议映射；`Block_Epoll_Net` 提供 Linux epoll 事件循环和线程池协作；`CLogic` 处理注册、登录、文件、目录、分享与续传协议；`CMysql` 访问 MySQL，文件内容由服务端文件系统保存。

## 关键组件

| 位置 | 职责 |
| --- | --- |
| `ckernel.cpp`、`ckernel.h` | 客户端核心：协议映射、登录后本地状态、传输任务与界面信号连接。 |
| `netapi/mediator/TcpClientMediator.*` | 客户端网络中介，对接内核与网络对象。 |
| `netapi/net/TcpClient.*` | Winsock 客户端连接、收发及接收线程。 |
| `sqlapi/csqlite.*` | 客户端 SQLite 任务记录访问。 |
| `NetDisk_server/NetDisk/src/TCPKernel.cpp`、`include/TCPKernel.h` | 服务端生命周期、MySQL 初始化、协议分发与网络层协调。 |
| `NetDisk_server/NetDisk/src/block_epoll_net.cpp`、`include/block_epoll_net.h` | 监听 socket、epoll 事件循环、收发与线程池协作。 |
| `NetDisk_server/NetDisk/src/clogic.cpp`、`include/clogic.h` | 服务端业务协议处理及文件系统操作。 |
| `NetDisk_server/NetDisk/src/Mysql.cpp`、`include/Mysql.h` | MySQL 客户端封装。 |

## 生成物与运行时数据边界

- 在源码树外创建构建目录；qmake、Make/NMake 生成的目标文件、Makefile 和可执行文件均为构建产物，不应作为源码修改目标。
- 客户端会在可执行文件相邻位置使用 `config.ini`，并创建 `database/` 保存按用户划分的 SQLite 传输任务数据库；这些都是运行时数据，应按部署和备份策略管理。
- 服务端 MySQL 数据与实际上传文件属于运行时持久化数据，不属于 `NetDisk_server/NetDisk` 的源码。当前文件逻辑使用源码定义的存储路径，部署前应确认其权限、容量和备份安排。
- 根目录 `src/` 与 `include/` 虽含旧服务端代码，但只是旧快照；它们既不是生成物，也不是服务端修改入口。

> 安全警告：当前服务端源码在 `NetDisk_server/NetDisk/include/packdef.h` 硬编码 MySQL `root/123456`，并在 `NetDisk_server/NetDisk/src/clogic.cpp` 硬编码存储根目录 `/home/coder/NetDisk/`。在后续阶段将这些值外置化之前，只能连接隔离的测试数据库并使用测试存储目录，禁止暴露或部署为生产服务；不要假设存在能够覆盖这些值的现有服务端配置文件。

服务端功能、修复、配置相关代码以及服务端测试的实现位置，统一限定为 `NetDisk_server/NetDisk`；若需迁移旧快照内容，先在该权威目录完成显式迁移，再修改权威实现。
