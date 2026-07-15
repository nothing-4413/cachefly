# cachefly

[![CI](https://github.com/nothing-4413/cachefly/actions/workflows/ci.yml/badge.svg)](https://github.com/nothing-4413/cachefly/actions/workflows/ci.yml)

`cachefly` 是一个使用 C++20 实现的 Redis RESP2 兼容缓存服务器，用于展示 Linux 网络编程、
并发分片、缓存淘汰、持久化和工程化测试能力。项目参考 DragonflyDB 的分层与
shared-nothing 思想，但不复制其源码，也不声称具备未经测量的性能数字。

## 关键设计

- 单 epoll Reactor 管理 accept、增量 RESP 解析和非阻塞 socket I/O；`eventfd` 接收跨线程任务。
- 全局命令 worker 池避免 Reactor 等待 shard future；同一连接串行执行并保持 pipeline 响应顺序，
  不同连接可并行执行。
- key 哈希路由到独占 KV 的 shard worker，支持 TTL、原子计数和 LRU/LFU/Random/NoEviction。
- 跨 shard MSET 使用 checkpoint/rollback 保证失败原子性，明确承担批量复制成本。
- AOF 支持 always/everysec/no，后台错误进入永久失败态；Snapshot 在原子替换前后同步文件和目录。
- 每连接限制活动客户端数、未消费请求和待发送响应；所有运行参数都能从 `/config` 审计。

## Redis 命令

`PING`、`ECHO`、`GET`、`SET`（EX/PX/NX/XX）、`DEL`、`EXISTS`、`EXPIRE`、`TTL`、
`MGET`、`MSET`、`INCR`、`DECR`。

## 构建与运行

要求 Linux、CMake 3.16+ 以及支持 C++20 的 GCC 或 Clang。

```bash
bash scripts/build.sh Release
bash scripts/run.sh
redis-cli -p 6379 PING
```

原生默认绑定 `127.0.0.1`。`scripts/run.sh` 支持通过 `CACHEFLY_BIND` 显式覆盖，其他配置使用
`--key=value` 覆盖。全部运行参数及其消费路径见[配置契约](docs/configuration.md)。

## 验证

GitHub Actions 执行严格编译警告、单元测试、Redis 客户端进程集成测试、20,000 请求并发 pipeline
稳定性测试、网络资源限制测试、ASan+UBSan、TSan 和最终 Docker 镜像构建。压测矩阵会记录 commit
与机器环境；只有在专用机器上运行后才应把吞吐或 P99 数字写入简历。

```bash
bash benchmark/matrix.sh ./build/src/cachefly
```

## 运行接口

- Redis RESP2：`127.0.0.1:6379`
- Prometheus metrics：`http://127.0.0.1:8080/metrics`
- 健康与持久化状态：`http://127.0.0.1:8080/status`
- 全量生效配置：`http://127.0.0.1:8080/config`

## 部署与边界

```bash
docker compose -f deploy/docker-compose.yml up --build -d
```

这是系统编程项目，不实现 AUTH、ACL、TLS、复制、集群或事务。原生和 Compose 默认只向本机
暴露端口；不要在没有独立访问控制层时发布到不可信网络。完整说明见 [`SECURITY.md`](SECURITY.md)
和[面试指南](docs/interview-guide.md)。

项目材料包括[运维手册](docs/operations.md)、[压测说明](benchmark/README.md)、
[简历 bullets](docs/resume.md)、[可靠性加固记录](docs/resume-hardening.md)，以及 11 个原始模块的
实现说明（`docs/module-*.md`）。
