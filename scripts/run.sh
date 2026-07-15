#!/usr/bin/env bash
set -euo pipefail

build_dir="${CACHEFLY_BUILD_DIR:-build}"
exec "${build_dir}/src/cachefly" --config=configs/cachefly.conf \
  --bind="${CACHEFLY_BIND:-127.0.0.1}" "$@"
