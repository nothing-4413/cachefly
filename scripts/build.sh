#!/usr/bin/env bash
set -euo pipefail

build_type="${1:-Debug}"
build_dir="${CACHEFLY_BUILD_DIR:-build}"
cmake -S . -B "${build_dir}" -DCMAKE_BUILD_TYPE="${build_type}" -DCACHEFLY_BUILD_TESTS=ON
cmake --build "${build_dir}" --parallel
ctest --test-dir "${build_dir}" --output-on-failure

