# cachefly

`cachefly` 是一个使用 C++20 实现的 Redis 协议兼容高性能缓存服务器。项目参考
DragonflyDB 的分层与 shared-nothing 思想，但不复制其源码。

## 构建

要求 Linux、CMake 3.16+ 以及支持 C++20 的 GCC 或 Clang。

```bash
bash scripts/build.sh
bash scripts/run.sh
```

## 模块进度

| 模块 | 内容 | 状态 |
|---|---|---|
| 1 | CMake、日志、配置、测试与 CI | 已完成 |
| 2 | epoll / Reactor 网络层 | 已完成 |
| 3 | RESP 协议 | 已完成 |
| 4 | Redis 命令 | 已完成 |
| 5 | 内存 KV 与 TTL | 已完成 |
| 6 | 多线程分片 | 已完成 |
| 7 | 缓存淘汰 | 已完成 |
| 8 | AOF 与 Snapshot | 已完成 |
| 9 | HTTP 管理端口与指标 | 已完成 |
| 10 | 压测与性能分析 | 已完成 |
| 11 | 部署、简历与面试材料 | 待实现 |

当前已完成模块 10，详细设计见
[`docs/module-10-benchmark.md`](docs/module-10-benchmark.md)。
