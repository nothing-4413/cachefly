#!/usr/bin/env bash
set -euo pipefail
host="${CACHEFLY_HOST:-127.0.0.1}"; port="${CACHEFLY_PORT:-6379}"
requests="${REQUESTS:-100000}"; clients="${CLIENTS:-50}"
pipeline="${PIPELINE:-1}"; value_size="${VALUE_SIZE:-64}"
output_dir="${OUTPUT_DIR:-benchmark/results/$(date +%Y%m%d-%H%M%S)}"
mkdir -p "${output_dir}"
{
  echo "date=$(date --iso-8601=seconds)"
  echo "host=${host} port=${port} requests=${requests} clients=${clients} pipeline=${pipeline} value_size=${value_size}"
  uname -a
  git rev-parse HEAD 2>/dev/null || true
} >"${output_dir}/environment.txt"
redis-benchmark -h "${host}" -p "${port}" -n "${requests}" -c "${clients}" \
  -P "${pipeline}" -d "${value_size}" -t set,get,incr --csv \
  | tee "${output_dir}/redis-benchmark.csv"
if command -v memtier_benchmark >/dev/null 2>&1; then
  memtier_benchmark --server="${host}" --port="${port}" --protocol=redis \
    --clients="${clients}" --requests="${requests}" --pipeline="${pipeline}" \
    --data-size="${value_size}" --json-out-file="${output_dir}/memtier.json"
fi
echo "results=${output_dir}"
