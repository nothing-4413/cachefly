# Resume material

## 中文

- 使用 C++20 从零实现 Redis RESP2 兼容缓存服务器，拆分 epoll Reactor、增量协议解析、
  命令注册、内存 KV、TTL、持久化和可观测性模块，并以 CMake/CTest/GitHub Actions 管理工程。
- 设计 shared-nothing 多线程分片，按 key 哈希路由到独占 KV 的 worker，原子实现 INCR/DECR，
  支持惰性与主动过期以及 LRU/LFU/Random/NoEviction 内存策略。
- 实现 RESP 格式 AOF、always/everysec/no fsync、原子 Snapshot 替换和启动恢复，提供
  Prometheus 延迟直方图、状态端口、Docker Compose 及可复现压测矩阵。

## English

- Built a C++20 Redis RESP2-compatible cache server with an epoll Reactor, incremental protocol
  parser, command registry, in-memory KV/TTL engine, persistence, and observability.
- Designed shared-nothing shard workers with key-affinity routing, atomic counters, active/lazy
  expiration, and per-shard LRU/LFU/Random/NoEviction memory policies.
- Added RESP AOF, configurable fsync, atomic snapshots and recovery, Prometheus histograms,
  Docker Compose deployment, CI integration tests, and reproducible benchmark matrices.

Only add throughput or P99 numbers after running `benchmark/matrix.sh` on a documented dedicated host.
