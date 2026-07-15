#!/usr/bin/env bash
set -euo pipefail

binary="${1:-./build/src/cachefly}"
log_file="${TMPDIR:-/tmp}/cachefly-concurrency.log"
"${binary}" --bind=127.0.0.1 --port=16380 --admin_port=18081 \
  --shard_threads=4 --maxmemory=128mb >"${log_file}" 2>&1 &
server_pid=$!
trap 'status=$?; kill "${server_pid}" 2>/dev/null || true; wait "${server_pid}" 2>/dev/null || true; if [ "${status}" -ne 0 ]; then cat "${log_file}"; fi; exit "${status}"' EXIT

for _ in {1..50}; do
  redis-cli -h 127.0.0.1 -p 16380 PING 2>/dev/null | grep -q PONG && break
  sleep 0.1
done

redis-benchmark -h 127.0.0.1 -p 16380 -n 20000 -c 50 -P 16 -t set,get,incr --csv
test "$(redis-cli -h 127.0.0.1 -p 16380 PING)" = "PONG"
curl --fail --silent http://127.0.0.1:18081/status | grep -q '"status":"ok"'
