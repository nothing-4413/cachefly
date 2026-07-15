# Operations guide

## Native

```bash
bash scripts/build.sh Release
bash scripts/run.sh
redis-cli -p 6379 PING
curl http://127.0.0.1:8080/status
```

Stop with SIGTERM so queued AOF records drain and an enabled snapshot is written. Keep AOF and
snapshot files on persistent storage. Restore is automatic: AOF is authoritative when enabled;
otherwise the snapshot is loaded.

Snapshot replacement synchronizes the temporary file and containing directory before and after the
atomic rename. An asynchronous AOF write or `fdatasync` failure permanently degrades the writer;
subsequent mutations return `MISCONF` before changing memory, and `/status` reports `degraded`.

## Docker Compose

```bash
docker compose -f deploy/docker-compose.yml up --build -d
docker compose -f deploy/docker-compose.yml ps
curl http://127.0.0.1:8080/metrics
```

Prometheus is available on port 9090. The named volume `cachefly-data` owns persistence files.
Back up the volume only after a clean stop or filesystem-level snapshot.

All Compose ports bind to host loopback by default. Cachefly does not implement AUTH, ACL, or TLS;
do not change these bindings to a public interface without a trusted private network, firewall, or
authenticated TLS proxy. The RESP and admin listeners currently share one configured bind address.

## Network limits

`max_clients` caps active RESP clients. `max_request_bytes` caps unread bytes held for one
client, including an incomplete request. `max_output_bytes` caps outstanding response bytes per
client across worker completions and the socket output buffer. A client exceeding any limit is
closed; rejected clients do not consume a command worker or shard task.

## Failure checks

- Startup failure: validate bind ports, persistence paths and directory permissions.
- Rising P99: compare shard queueing, CPU saturation, context switches and value size.
- OOM replies: inspect maxmemory and eviction policy; NoEviction intentionally rejects writes.
- Client disconnects: inspect the server log for connection, request, or output limit messages.
- Corrupt AOF: preserve the file, locate the reported byte offset and truncate only an incomplete tail.
