# Performance benchmark

Build `Release`, start cachefly, then run `bash benchmark/run.sh`. Environment variables control
requests, clients, pipeline, value size, host, port and output directory. Each run stores the Git
revision and host metadata beside CSV/JSON output.

`bash benchmark/matrix.sh ./build/src/cachefly` compares 1/2/4/8 shards and pipeline depths
1/8/32. Use an isolated Linux host with fixed CPU frequency. CI samples are correctness checks,
not capacity numbers.
