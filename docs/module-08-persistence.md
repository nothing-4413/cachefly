# 模块 8：AOF 与 Snapshot

AOF 使用独立写线程和 RESP 命令格式。`always` 等待写入及 `fdatasync`，`everysec` 每秒
同步，`no` 交给操作系统。关闭时队列会完整排空；恢复允许忽略最后一个不完整帧，但中间
协议损坏会使启动失败。

Snapshot 将当前存活键编码为 SET/PX 命令，先写 `.tmp`，刷新成功后原子重命名。启用
AOF 时 AOF 是唯一恢复来源；否则启用 Snapshot 时加载快照，避免全量 AOF 与快照重复回放。

完成度：模块 8/11，项目约 77%。
