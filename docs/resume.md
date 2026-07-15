# Resume material

## 中文

- 使用 C++20 实现 Redis RESP2 兼容缓存服务器：单 epoll Reactor 负责非阻塞 I/O 与增量解析，
  命令 worker 池异步执行，并以连接级串行队列保持 pipeline 响应顺序。
- 设计 shared-nothing 多线程分片，按 key 哈希路由到独占 KV worker，支持原子计数、TTL、
  LRU/LFU/Random/NoEviction；跨 shard MSET 通过 checkpoint/rollback 保证失败原子性。
- 实现 RESP AOF、always/everysec/no fsync、文件与目录同步的原子 Snapshot；后台持久化故障
  可进入永久失败态、反映到健康接口，并在后续写入修改内存前返回 MISCONF。
- 建立可审计配置与网络资源上限，使用 CMake/CTest/GitHub Actions 验证严格警告、进程集成、
  20,000 请求并发 pipeline、ASan/UBSan/TSan、Docker 构建及可复现压测矩阵。

## English

- Built a C++20 Redis RESP2-compatible cache server with a single epoll Reactor for nonblocking I/O
  and incremental parsing, plus asynchronous command workers and per-connection ordered pipelines.
- Designed shared-nothing shard workers with key-affinity routing, atomic counters, active/lazy TTL,
  LRU/LFU/Random/NoEviction, and checkpoint/rollback semantics for cross-shard MSET failures.
- Implemented RESP AOF with configurable fsync, durable atomic snapshot replacement, persistent
  background-error reporting, health degradation, and pre-mutation rejection after known failures.
- Added auditable runtime limits and CI coverage for strict warnings, process integration, a 20,000-
  request concurrent pipeline run, ASan/UBSan/TSan, Docker builds, and reproducible benchmark matrices.

Only add throughput or P99 numbers after running `benchmark/matrix.sh` on a documented dedicated host.
