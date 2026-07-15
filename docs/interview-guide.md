# Interview guide

## Why epoll plus eventfd?

The Reactor owns every fd registration and output buffer. Worker threads enqueue callbacks and write
eventfd, so they never mutate epoll state concurrently. This reduces ownership ambiguity and lock scope.

## How are TCP sticky packets and partial packets handled?

`Buffer` retains unread bytes. The RESP parser reports complete/incomplete/error and consumes only one
complete frame, so a callback can loop through pipelines while preserving a partial tail.

## Why shared-nothing shards?

Each key maps to one worker that exclusively owns its hash table. Single-key operations need no storage
lock and INCR is atomic by serialization. A connection-ordered dispatcher keeps shard waits off the
Reactor, while different connections execute concurrently.

## TTL correctness

Deadlines use `steady_clock`. Access paths lazily remove expired keys; each shard also performs a bounded,
rotating active scan. Redis-compatible TTL returns -2 for absent and -1 for persistent keys.

## Eviction trade-offs

Global maxmemory is divided among shards to avoid a global lock. This can leave capacity stranded in a
cold shard. LRU uses a logical access clock, LFU a saturating counter, and Random avoids metadata cost.

## AOF and snapshot recovery

AOF records are RESP commands. Always waits for fdatasync; everysec batches it. Snapshot writes a temporary
file and atomically renames it. AOF is authoritative when enabled to prevent double replay of increments.

## How is P99 measured?

The executor records microseconds in cumulative Prometheus buckets. Client-side memtier percentiles and
server buckets are compared; divergence often indicates network/client queueing rather than execution time.

## Current limitations

- One network Reactor; command execution is asynchronous, but socket I/O itself is not yet multi-Reactor.
- RESP2 string values only; no transactions, replication, clustering, ACL, Lua, streams, or complex types.
- No AUTH or TLS; native and Compose defaults are loopback-only and public exposure requires an
  external authenticated transport boundary.
- Cross-shard MSET uses checkpoint/rollback for atomic failure semantics; this copies touched shard
  state and is intentionally optimized for correctness rather than large-batch throughput.
- Per-shard memory budgets can be imbalanced and leave capacity stranded in a cold shard.
- Snapshot occurs on clean shutdown rather than background fork/copy-on-write.

These constraints are deliberate and should be stated before proposing multi-Reactor async continuations,
cross-shard coordination, richer Redis types, or production security.
