# 模块 6：shared-nothing 分片

每个 shard 拥有独立线程、任务队列和 `KvStore`。key 通过稳定哈希映射到唯一 shard，
所以单 key 的 SET/GET/INCR/TTL 严格串行，存储对象内部不需要互斥锁。

跨 key 命令由 `ShardedDatabase` 门面拆分并汇总。主动过期由各 shard 每 100ms 执行，
不再占用 Reactor。当前同步数据库门面保证命令顺序；后续性能阶段会量化任务切换成本。

完成度：模块 6/11，项目约 58%。
