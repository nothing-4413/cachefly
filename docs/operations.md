# Operations guide

## Native

```bash
bash scripts/build.sh Release
./build/src/cachefly --config=configs/cachefly.conf
redis-cli -p 6379 PING
curl http://127.0.0.1:8080/status
```

Stop with SIGTERM so queued AOF records drain and an enabled snapshot is written. Keep AOF and
snapshot files on persistent storage. Restore is automatic: AOF is authoritative when enabled;
otherwise the snapshot is loaded.

## Docker Compose

```bash
docker compose -f deploy/docker-compose.yml up --build -d
docker compose -f deploy/docker-compose.yml ps
curl http://127.0.0.1:8080/metrics
```

Prometheus is available on port 9090. The named volume `cachefly-data` owns persistence files.
Back up the volume only after a clean stop or filesystem-level snapshot.

## Failure checks

- Startup failure: validate bind ports, persistence paths and directory permissions.
- Rising P99: compare shard queueing, CPU saturation, context switches and value size.
- OOM replies: inspect maxmemory and eviction policy; NoEviction intentionally rejects writes.
- Corrupt AOF: preserve the file, locate the reported byte offset and truncate only an incomplete tail.
