# Resume hardening

This pass removes configuration that did not affect runtime behavior and closes correctness gaps that
are likely to be challenged in an interview:

- Removed the unused `io_threads` option instead of advertising an unimplemented multi-Reactor.
- Shard calls use packaged tasks, so allocation and execution failures propagate through futures.
- AOF recovery truncates an incomplete tail before opening the file for new appends.
- MSET is one database operation. A shard validates a complete batch before commit; cross-shard failure
  restores checkpoints, and AOF records the command only after the whole operation succeeds.

The next architectural step is asynchronous command continuations so the Reactor never waits on shard
futures. Until that lands, the project should claim shared-nothing storage ownership, not a nonblocking
end-to-end execution path.
