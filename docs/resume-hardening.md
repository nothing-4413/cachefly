# Resume hardening

This pass removes configuration that did not affect runtime behavior and closes correctness gaps that
are likely to be challenged in an interview:

- Removed the unused `io_threads` option instead of advertising an unimplemented multi-Reactor.
- Shard calls use packaged tasks, so allocation and execution failures propagate through futures.
- AOF recovery truncates an incomplete tail before opening the file for new appends.
- MSET is one database operation. A shard validates a complete batch before commit; cross-shard failure
  restores checkpoints, and AOF records the command only after the whole operation succeeds.

The command path now uses a worker dispatcher: different connections execute concurrently, commands from
one connection remain ordered, and replies return through `TcpConnection::Send`/`EventLoop::QueueInLoop`.
The Reactor parses frames and enqueues work but never waits on shard futures. Shutdown drains accepted
commands before saving a snapshot.

CI exercises strict warnings, ASan+UBSan, TSan, process-level Redis compatibility, a 20,000-request
concurrent pipeline run, and the deployment image. Sanitizer options are real build inputs rather than
unused switches.
