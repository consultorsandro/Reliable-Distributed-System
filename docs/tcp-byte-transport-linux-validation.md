# Linux/POSIX TCP byte transport validation — 2026-10-17

Working directory: `/home/veterano_linux/Reliable-Distributed-System`.

## Scope and implementation

`src/transport/tcp_posix.cpp` now implements the existing
`TcpConnection::sendAll(std::span<const std::byte>)` and
`TcpConnection::receive(std::span<std::byte>)` interfaces. No public signature,
ownership rule, framing or serialization code, connection-establishment policy, or
application behavior changed. `SocketHandle` remains the sole native socket owner and
continues to perform cleanup after an I/O failure.

`sendAll` makes no native call for empty input. For nonempty input it caps each native
length at `SSIZE_MAX`, uses the POSIX `ssize_t` result, advances by every positive short
send, and continues until the complete span is accepted. A zero-progress result throws
`std::runtime_error` instead of looping. A `-1` result with `EINTR` retries that native
operation without losing the successfully transmitted prefix. Other failures capture
`errno` immediately and throw `std::system_error` with `std::system_category()`.

Every Linux `send` uses `MSG_NOSIGNAL`; no process-wide SIGPIPE handler or disposition
change was introduced. `receive` rejects an empty span with `std::invalid_argument`,
caps a nonempty read at `SSIZE_MAX`, retries `EINTR`, and returns after one successful
native `recv`. Its return is the actual byte count, including zero for orderly EOF.
Neither method adds timeouts, reconnection, or application-level retry.

## Files and historical Windows boundary

Modified:

- `src/transport/tcp_posix.cpp`
- `include/rds/transport/tcp_connection.hpp` (comment clarification only)
- `tests/tcp_byte_transport_tests.cpp`
- `tests/CMakeLists.txt`
- `docs/tcp-byte-transport.md`
- `docs/linux-native-socket-ownership.md`
- `docs/tcp-establishment-linux-validation.md`

Created: `docs/tcp-byte-transport-linux-validation.md`. No file was deleted or renamed.

The original Windows implementation remains unchanged:

- `src/transport/tcp.cpp`: `TcpConnection::sendAll` and
  `TcpConnection::receive`, plus Winsock establishment.
- `src/transport/socket_handle.cpp`: `SOCKET` ownership and `closesocket` cleanup.
- `src/transport/winsock_runtime.cpp`: `WSAStartup`/`WSACleanup` lifecycle.

`docs/tcp-byte-transport-validation.md` remains the unmodified historical Windows/MSVC
validation record. Windows was not rebuilt or rerun in this Linux session.

## Tests and exact results

The existing eight byte-transport cases are now portable and enabled on Linux: empty
send, empty receive rejection, binary bytes, bidirectional transfer, repeated bounded
reads, sequential sends, orderly EOF, and moved-from behavior. Windows keeps those same
cases and native `WSAENOTSOCK` expectation. Linux expects `EBADF` for moved-from I/O.

A Linux-only test configures the peer for an abortive close, waits at most two seconds
for the reset with `poll`, consumes the reset indication, then sends again. It observed
`EPIPE` through `std::system_error`; the test process remained alive, directly evidencing
per-call SIGPIPE suppression. Every socket test retains a 20-second CTest timeout.

Commands:

```text
cmake -S . -B build-linux
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

Configure: exit 0. Build: exit 0, including `rds_transport` and
`tcp_byte_transport_tests`.

The first CTest run inside the restricted sandbox exited 8: **27 executed, 4 passed,
23 failed**, 0.08 seconds. Every failure required native socket creation and reported
`EPERM` (`Operation not permitted`). This was an environmental restriction, not an
application failure.

The identical CTest command was rerun with native socket permission: exit 0,
**27 executed, 27 passed, 0 failed**, 0.08 seconds. All nine byte-transport cases passed,
including `tcp_bytes_sigpipe_suppression`.

## Evidence boundaries and limitations

Verified on Linux: all specified portable byte behaviors, bounded repeated reads,
orderly EOF, moved-from native errors, continued moved-to ownership, `EPIPE` propagation,
and survival of the post-peer-close send. The production code's positive-short-send and
zero-progress branches were reviewed but not deterministically induced. Multiple reads
were forced by buffer capacity and do not prove TCP packet boundaries.

`EINTR` handling is implemented for both calls but was not directly fault-injected.
A deterministic blocked-send interruption test would require intrusive coordination and
socket-buffer assumptions; no mock seam or process-wide production signal policy was
added for this increment. Also untested: spans above `SSIZE_MAX`, failure after partial
acceptance, external networks, resource exhaustion, Release builds, and Windows behavior
in this session.

No B5 PING/PONG exchange, asynchronous I/O, retry/reconnect policy, heartbeat, framing
change, or third-party dependency was added. The next recommended increment is to enable
and validate the existing framing-over-TCP tests on Linux without changing the protocol.
