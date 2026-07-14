# 模块 5：内存 KV 与 TTL

`KvStore` 实现命令层的 `Database` 契约，当前由 Reactor 线程独占访问，不需要内部锁。
Entry 将 value 与绝对过期时间放在一起，过期时间使用 `steady_clock`，不受系统时间调整影响。

过期删除采用两条路径：GET/SET/DEL/EXISTS/TTL/INCR 访问时惰性删除；每条命令后再执行
有最大采样数的主动扫描。扫描位置轮转，避免每轮只检查哈希表头部导致后部键饥饿。

主程序已串联 TCP、RESP、命令和存储层，可以使用 `redis-cli -p 6379` 执行当前支持的命令。

完成度：模块 5/11，项目约 47%。
