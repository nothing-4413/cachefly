# 模块 7：内存限制与淘汰

全局 `maxmemory` 按 shard 均分。每个 shard 独立执行 LRU、LFU、Random 或 NoEviction，
避免跨线程维护全局淘汰链表。内存统计包含 Entry、key 和 value 的近似占用。

写入先计算替换后的投影值，再淘汰其他键；如果仍无法容纳，旧值保持不变。LRU 使用
逻辑访问序号，LFU 使用饱和计数器，Random 使用 shard 内部 PRNG。

完成度：模块 7/11，项目约 67%。
