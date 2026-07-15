# Performance report protocol

Record CPU, core count, RAM, kernel, compiler, commit and client placement. For every scenario
report requests/sec, average latency and P50/P95/P99.

| Variable | Values |
|---|---|
| shard threads | 1, 2, 4, 8 |
| pipeline | 1, 8, 32 |
| value size | 64 B, 1 KiB, 16 KiB |
| clients | 1, 10, 50, 200 |
| eviction | noeviction, LRU, LFU, random |

Attach raw result files. Investigate regressions with `perf stat`, `perf record`, context switches
and the cachefly latency histogram before changing the architecture.
