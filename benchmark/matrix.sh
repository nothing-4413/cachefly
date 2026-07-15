#!/usr/bin/env bash
set -euo pipefail
binary="${1:-./build/src/cachefly}"
root="${OUTPUT_DIR:-benchmark/results/matrix-$(date +%Y%m%d-%H%M%S)}"
mkdir -p "${root}"
for shards in 1 2 4 8; do
  for pipeline in 1 8 32; do
    port=$((17000 + shards * 100 + pipeline))
    "${binary}" --bind=127.0.0.1 --port="${port}" --admin_port=$((port + 1)) \
      --shard_threads="${shards}" --maxmemory=1gb --eviction_policy=lru \
      >"${root}/server-${shards}-${pipeline}.log" 2>&1 &
    pid=$!
    trap 'kill "${pid}" 2>/dev/null || true' EXIT
    for _ in {1..50}; do redis-cli -p "${port}" PING >/dev/null 2>&1 && break; sleep 0.1; done
    CACHEFLY_PORT="${port}" PIPELINE="${pipeline}" \
      OUTPUT_DIR="${root}/shards-${shards}-pipeline-${pipeline}" bash benchmark/run.sh
    kill "${pid}"; wait "${pid}" || true
    trap - EXIT
  done
done
