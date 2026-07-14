# 模块 3：RESP2 协议

解析器支持 Simple String、Error、Integer、Bulk String、Array 和 Null，并提供对称编码。

解析采用三态结果：

- `kComplete`：只消费一个完整帧，剩余字节留给 pipeline 下一条命令。
- `kIncomplete`：消费量为 0，等待 TCP 后续数据，保证半包不会丢失。
- `kError`：返回稳定的协议错误，调用方可以回复错误后关闭连接。

解析器限制 Bulk 长度、数组元素数和嵌套深度，避免长度字段导致过量分配或递归攻击。
`ParseCommand` 进一步要求顶层是非空数组且参数都是字符串。

完成度：模块 3/11，项目约 27%。
