# 模块 2：Linux 网络层

## 设计

网络层采用单 Reactor：`EventLoop` 独占 `epoll`，`Channel` 表示 fd 的关注事件，
`TcpConnection` 管理连接状态和读写缓冲，`TcpServer` 管理监听与连接集合。

其他线程不能直接修改 `epoll` 或连接输出缓冲。它们通过 `QueueInLoop` 投递任务，
`eventfd` 唤醒 Reactor。这一约束为后续 shard 工作线程异步返回 RESP 响应提供安全边界。

`Buffer::ReadFd` 使用 `readv` 的栈上额外缓冲，一次系统调用即可接收超过当前可写空间的
数据。未消费数据始终保留，因此天然支持 TCP 半包和粘包。

## 验收

- 缓冲区扩容和部分消费测试
- `socketpair` 大包读取测试
- 跨线程任务投递和 `eventfd` 唤醒测试
- GitHub Actions Linux 编译与 CTest

完成度：模块 2/11，项目约 18%。
