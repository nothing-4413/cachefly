# 模块 11：部署、简历与面试材料

交付包括多阶段非 root Docker 镜像、持久化 Compose、Prometheus、健康检查、运维手册、
中英文简历 bullet 和面试问答。文档明确当前限制，不使用未经专用机器验证的性能数字。

CI 在单元和进程集成测试后构建最终 Docker 镜像，保证部署文件不会长期失效。

后续可靠性加固增加 ASan/UBSan/TSan、并发 pipeline 与网络边界测试、持久化错误状态、
durable Snapshot 替换、loopback 安全默认值和全量配置契约。简历材料只陈述 CI 或原始结果可验证的内容。

完成度：模块 11/11，项目 100%。
