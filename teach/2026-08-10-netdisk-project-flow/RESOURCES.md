# NetDisk 项目全流程资源

## Knowledge

- [项目客户端业务核心](../../../ckernel.cpp)
  当前项目的一手材料。用于核对客户端启动、信号连接、协议分发、上传下载和 SQLite 任务恢复。
- [项目服务端业务核心](../../../NetDisk_server/NetDisk/src/clogic.cpp)
  当前项目的一手材料。用于核对认证、目录、上传下载、秒传、分享、删除和续传逻辑。
- [项目服务端网络层](../../../NetDisk_server/NetDisk/src/block_epoll_net.cpp)
  当前项目的一手材料。用于核对 epoll、EPOLLONESHOT、包长读取、线程池投递与发送锁。
- [Qt Signals & Slots 官方文档](https://doc.qt.io/qt-6.5/signalsandslots.html)
  解释信号槽的松耦合、类型检查、直接连接与队列连接。用于理解客户端跨线程事件链。
- [Linux epoll(7) 手册](https://man7.org/linux/man-pages/man7/epoll.7.html)
  epoll、LT/ET 与 EPOLLONESHOT 的权威行为说明。用于校正“用了 epoll 就是全非阻塞 Reactor”的误解。
- [TCP 标准 RFC 9293](https://www.rfc-editor.org/rfc/rfc9293.html)
  TCP 标准。用于理解 TCP 提供可靠有序字节流，但应用层仍需自行定义消息边界。
- [MySQL C API Prepared Statement 官方文档](https://dev.mysql.com/doc/c-api/8.0/en/c-api-prepared-statement-interface.html)
  说明 `mysql_stmt_prepare`、参数绑定、执行和结果读取。用于理解本项目预处理封装。
- [C++ Core Guidelines：资源管理](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource)
  RAII、所有权和资源生命周期的权威工程建议。用于评估项目中的裸指针、文件描述符和锁守卫。

## Wisdom (Communities)

- [Stack Overflow：C++ 标签](https://stackoverflow.com/questions/tagged/c%2b%2b)
  用于把具体的对象生命周期、ABI、Qt 信号槽问题整理成最小复现后接受同行检验；不要把投票答案当标准本身。
- [MySQL Community Forums](https://forums.mysql.com/)
  用于验证 MySQL C API、事务和连接并发的真实工程问题。

## Gaps

- 仓库缺少首次创建 `t_user`、`t_file`、`t_user_file` 和 `user_file_info` 的完整 schema。
- 仓库缺少可复现的并发压测报告，无法证明具体连接数或吞吐量。
- 当前环境没有与项目一致的 Qt 5/MinGW 与 Linux MySQL 开发环境，源码尚未在匹配工具链完成构建验证。
