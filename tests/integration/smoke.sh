#!/usr/bin/env bash
set -euo pipefail
binary="${1:-./build/src/cachefly}"
mode="${2:-all}"
log_file="${TMPDIR:-/tmp}/cachefly-smoke.log"
"${binary}" --bind=127.0.0.1 --port=16379 --admin_port=18080 \
  --shard_threads=2 --maxmemory=64mb >"${log_file}" 2>&1 &
server_pid=$!
trap 'kill "${server_pid}" 2>/dev/null || true; wait "${server_pid}" 2>/dev/null || true' EXIT
for _ in {1..40}; do
  redis-cli -h 127.0.0.1 -p 16379 PING 2>/dev/null | grep -q PONG && break
  sleep 0.1
done
if [[ "${mode}" == all || "${mode}" == basic ]]; then
  test "$(redis-cli -h 127.0.0.1 -p 16379 SET smoke value)" = "OK"
  test "$(redis-cli -h 127.0.0.1 -p 16379 GET smoke)" = "value"
  test "$(redis-cli -h 127.0.0.1 -p 16379 INCR counter)" = "1"
fi
if [[ "${mode}" == all || "${mode}" == pipeline ]]; then
  redis-benchmark -h 127.0.0.1 -p 16379 -n 100 -c 2 -P 8 -t set --csv
fi
if [[ "${mode}" == all || "${mode}" == admin ]]; then
  curl --fail --silent http://127.0.0.1:18080/metrics | grep -q cachefly_commands_total
  curl --fail --silent http://127.0.0.1:18080/status | grep -q '"status":"ok"'
fi
if [[ "${mode}" == all || "${mode}" == benchmark ]]; then
  redis-benchmark -h 127.0.0.1 -p 16379 -n 1000 -c 10 -P 4 -t set,get --csv
fi
