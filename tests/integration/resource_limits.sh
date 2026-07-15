#!/usr/bin/env bash
set -euo pipefail

binary="${1:-./build/src/cachefly}"
log_file="${TMPDIR:-/tmp}/cachefly-resource-limits.log"
"${binary}" --bind=127.0.0.1 --port=16382 --admin_port=18083 \
  --shard_threads=2 --maxmemory=32mb --max_clients=1 \
  --max_request_bytes=4kb --max_output_bytes=512b >"${log_file}" 2>&1 &
server_pid=$!
trap 'status=$?; kill "${server_pid}" 2>/dev/null || true; wait "${server_pid}" 2>/dev/null || true; if [ "${status}" -ne 0 ]; then cat "${log_file}"; fi; exit "${status}"' EXIT

for _ in {1..50}; do
  redis-cli -h 127.0.0.1 -p 16382 PING 2>/dev/null | grep -q PONG && break
  sleep 0.1
done

python3 - <<'PY'
import socket
import time

ADDRESS = ("127.0.0.1", 16382)


def connect():
    sock = socket.create_connection(ADDRESS, timeout=2)
    sock.settimeout(2)
    return sock


def expect_closed(sock):
    try:
        data = sock.recv(4096)
    except (ConnectionResetError, BrokenPipeError):
        return
    if data:
        raise AssertionError(f"expected closed connection, received {data!r}")


def wait_until_available():
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        try:
            sock = connect()
            sock.sendall(b"*1\r\n$4\r\nPING\r\n")
            if sock.recv(64) == b"+PONG\r\n":
                return sock
            sock.close()
        except OSError:
            time.sleep(0.05)
    raise AssertionError("server did not accept a client before the deadline")


first = wait_until_available()
second = connect()
second.sendall(b"*1\r\n$4\r\nPING\r\n")
expect_closed(second)
second.close()
first.close()

oversized = wait_until_available()
oversized.sendall(b"x" * 4097)
expect_closed(oversized)
oversized.close()

value = b"v" * 1024
client = wait_until_available()
client.sendall(b"*3\r\n$3\r\nSET\r\n$3\r\nbig\r\n$1024\r\n" + value + b"\r\n")
assert client.recv(64) == b"+OK\r\n"
client.sendall(b"*2\r\n$3\r\nGET\r\n$3\r\nbig\r\n")
expect_closed(client)
client.close()
PY
