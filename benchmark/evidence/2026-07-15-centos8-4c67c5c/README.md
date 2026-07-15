# CentOS 8 validation evidence

This directory contains the raw validation output for commit
`4c67c5c20a0f9ebf1f0c05dfcc101c6155281eaf`, tested on 2026-07-15.

## Provenance

- Source was transferred as a complete `git bundle` because the VM's GitHub HTTPS connection was
  unstable. Local and remote SHA-256 both matched
  `8c0fb9c01f4dacd05dbe021dfc3f759e0a0551883b1a4fbf6bcf1728255090ef`.
- The Linux checkout reported the exact tested commit and no tracked-file modifications.
- Shell scripts were verified as LF and passed `bash -n` before execution.

## Environment

- CentOS Linux 8, kernel 4.18, VMware virtual machine
- 8 vCPU backed by an Intel Core i9-14900HX; 2.7 GiB guest RAM
- Clang 12.0.1 with the CentOS 8 libstdc++ 8 runtime
- CMake 3.20.2; redis-benchmark 8.6.2
- Source and builds used `/dev/shm` because the guest root filesystem was 99% full

The complete machine metadata is in [`environment.txt`](environment.txt).

## Validation result

| Check | Result |
|---|---|
| Release build with warnings as errors | PASS |
| Unit cases registered in the test executable | 43, PASS |
| Basic Redis commands | PASS |
| Ordered pipeline smoke | PASS |
| HTTP metrics/status | PASS |
| Benchmark smoke | PASS |
| 20,000-request concurrent pipeline stability | PASS |
| Client/request/output resource limits | PASS |
| ASan + UBSan | PASS |
| TSan | PASS |
| 12-scenario Release benchmark matrix | PASS |

See [`test-summary.txt`](test-summary.txt) and the corresponding build/test logs in this directory.

## Benchmark method

Each matrix cell used 100,000 requests, 50 clients, 64-byte values, LRU, and one of 1/2/4/8 shard
workers with pipeline depth 1/8/32. The table shows requests/sec for each command and the highest P99
among SET, GET, and INCR in that cell.

| Shards | Pipeline | SET req/s | GET req/s | INCR req/s | Max P99 ms |
|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 23,402.76 | 21,413.28 | 21,630.97 | 3.735 |
| 1 | 8 | 9,690.86 | 9,582.22 | 9,563.89 | 9.927 |
| 1 | 32 | 17,382.24 | 17,262.21 | 17,605.63 | 88.511 |
| 2 | 1 | 35,829.45 | 38,639.88 | 38,343.56 | 2.407 |
| 2 | 8 | 9,709.68 | 9,631.13 | 9,706.85 | 2.607 |
| 2 | 32 | 33,898.30 | 33,967.39 | 33,014.20 | 43.327 |
| 4 | 1 | 68,166.33 | 64,102.57 | 68,634.18 | 1.671 |
| 4 | 8 | 9,722.90 | 9,658.10 | 9,719.12 | 1.935 |
| 4 | 32 | 37,778.62 | 37,678.97 | 37,750.09 | 19.759 |
| 8 | 1 | 59,347.18 | 60,313.63 | 60,132.29 | 1.991 |
| 8 | 8 | 9,704.97 | 9,695.56 | 9,702.14 | 2.167 |
| 8 | 32 | 37,936.27 | 37,439.16 | 37,467.22 | 18.511 |

[`benchmark-summary.csv`](benchmark-summary.csv) contains all 36 command rows. The
[`benchmark-matrix`](benchmark-matrix/) subdirectory contains every raw CSV, per-scenario environment
record, and server log.

## Interpretation and limits

- The best result on this VM was 4 shards with pipeline depth 1. Eight shards did not improve the
  peak, which is consistent with scheduling and queueing overhead on an 8-vCPU guest.
- Pipeline depth was not monotonic in this run. The raw results are retained rather than selecting
  only favorable cells; profiling is required before attributing the pipeline-8 plateau to one cause.
- `memtier_benchmark` was not installed, so only redis-benchmark results are reported.
- Docker was not rebuilt on this VM because its root filesystem had only about 204 MiB free. The
  repository's GitHub Actions run remains the Docker build authority.
- These numbers are evidence for this environment, not a production capacity claim or a comparison
  with Redis/DragonflyDB on equivalent dedicated hardware.
