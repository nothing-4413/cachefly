# 模块 10：压测与性能分析

仓库提供可参数化的 redis-benchmark/memtier 入口、shard/pipeline 矩阵、结果目录和报告
协议。手动 GitHub workflow 使用 Release 构建并上传原始 artifact。

普通 CI 会启动真实进程，用 redis-cli 验证命令与 pipeline，检查管理端口，再执行 1000
请求 smoke benchmark。CI 只证明兼容性与稳定性，不冒充专用机器性能数据。

完成度：模块 10/11，项目约 93%。
