# Configuration contract

Every accepted key below has a runtime consumer and a behavior test or integration path. Unknown
keys fail startup. Command-line values use `--key=value` and override the config file.

| Key | Native default | Runtime effect |
|---|---:|---|
| `bind` | `127.0.0.1` | Bind address for RESP and admin listeners |
| `port` | `6379` | RESP listener port |
| `shard_threads` | `4` | Shard ownership workers and asynchronous command workers |
| `max_clients` | `10000` | Maximum active RESP connections |
| `max_request_bytes` | `16mb` | Per-client unread input cap before protocol dispatch |
| `max_output_bytes` | `64mb` | Per-client outstanding response cap, including worker callbacks |
| `maxmemory` | `512mb` | Memory budget divided across shards |
| `eviction_policy` | `lru` | LRU, LFU, random, or noeviction write behavior |
| `log_level` | `info` | Logger severity threshold |
| `log_file` | empty | Empty writes to stderr; a path redirects logger output |
| `appendonly` | `false` | Enables AOF recovery and mutation recording |
| `appendfilename` | `cachefly.aof` | AOF recovery and append path |
| `appendfsync` | `everysec` | AOF durability policy: always, everysec, or no |
| `snapshot` | `false` | Enables snapshot recovery and clean-shutdown save |
| `snapshotfilename` | `cachefly.snapshot` | Snapshot recovery and replacement path |
| `admin_port` | `8080` | HTTP metrics, status, and effective-config listener port |

`configs/cachefly.conf` is the container profile, so it intentionally changes `bind` to `0.0.0.0`.
Compose publishes that container listener only on host loopback. `GET /config` returns every effective
runtime field, with memory sizes normalized to bytes.
