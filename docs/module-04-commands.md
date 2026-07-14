# 模块 4：Redis 命令层

命令注册表保存命令名、参数范围、读写属性和处理函数。执行器统一完成大小写归一化、
参数校验、Redis 语义和 RESP 返回，不让网络层出现命令判断分支。

存储依赖被收敛为 `Database` 接口，其中 `Increment` 是原子操作，而不是 GET 后 SET。
这使模块 5 可以接入真实 KV/TTL，引入并发后也不会破坏 INCR/DECR 语义。

已支持：PING、ECHO、GET、SET（EX/PX/NX/XX）、DEL、EXISTS、EXPIRE、TTL、MGET、
MSET、INCR、DECR。错误返回覆盖未知命令、参数个数、整数范围、语法和内存不足。

完成度：模块 4/11，项目约 36%。
