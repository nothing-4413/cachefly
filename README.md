# cachefly

`cachefly` 是一个使用 C++20 实现的 Redis 协议兼容高性能缓存服务器。项目参考
DragonflyDB 的分层与 shared-nothing 思想，但不复制其源码。

这是用于展示系统编程与缓存架构能力的项目，不实现 Redis AUTH、ACL 或 TLS。原生和
Docker Compose 默认仅向本机暴露端口；不要在没有独立访问控制层时发布到不可信网络。

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
| 11 | 部署、简历与面试材料 | 已完成 |

## 运行接口

- Redis RESP2：`6379`
- Prometheus metrics：`http://127.0.0.1:8080/metrics`
- 状态：`http://127.0.0.1:8080/status`
- 生效配置：`http://127.0.0.1:8080/config`

## 部署与项目材料

```bash
docker compose -f deploy/docker-compose.yml up --build -d
```

参见[运维手册](docs/operations.md)、[压测说明](benchmark/README.md)、
[简历材料](docs/resume.md)和[面试指南](docs/interview-guide.md)。
安全边界与部署要求见 [`SECURITY.md`](SECURITY.md)。
全部运行参数及其实际消费路径见[配置契约](docs/configuration.md)。

全部 11 个模块已完成。最终交付说明见
[`docs/module-11-delivery.md`](docs/module-11-delivery.md)。
