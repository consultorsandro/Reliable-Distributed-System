# Linux/POSIX TCP establishment validation — 2026-10-17

Working directory: `/home/veterano_linux/Reliable-Distributed-System`.

## Scope and files

Created `src/transport/tcp_posix.cpp` and this report. Modified root
`CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/tcp_establishment_tests.cpp`, the
two TCP public-header comments, `docs/tcp-establishment.md`, and
`docs/linux-native-socket-ownership.md`. No file was deleted or renamed.

The POSIX source implements `TcpListener(address, port)`, `localPort`, `accept`,
and `connectTcp`. Public signatures and move-only `SocketHandle` ownership are
unchanged. It does not implement Linux `sendAll` or `receive`.

The preserved Windows files have these full paths and responsibilities:

- `/home/veterano_linux/Reliable-Distributed-System/src/transport/tcp.cpp`:
  Winsock endpoint parsing, listener/connection establishment,
  `SO_EXCLUSIVEADDRUSE`, `sendAll`, and `receive`.
- `/home/veterano_linux/Reliable-Distributed-System/src/transport/socket_handle.cpp`:
  exclusive `SOCKET` ownership and `closesocket` cleanup.
- `/home/veterano_linux/Reliable-Distributed-System/src/transport/winsock_runtime.cpp`:
  `WSAStartup`/`WSACleanup` lifecycle.

They and the historical Windows validation report were preserved but were not
built or revalidated during this Linux session.

## Policy and error behavior

Linux uses plain `bind`, without `SO_REUSEADDR` or `SO_REUSEPORT`; the duplicate
active loopback listener test observed `EADDRINUSE`. Windows continues to use
`SO_EXCLUSIVEADDRUSE`. The policies do not give identical exclusivity guarantees,
and immediate rebinding after closure can be limited by TCP state and kernel rules.

POSIX failures capture `errno` immediately and use `std::system_category()`.
Tests expect symbolic POSIX errors rather than portable numeric values. `accept`
and `connect` expose `EINTR` without an automatic retry policy.

## Commands and exact results

1. `cmake -S . -B build-linux`: exit 0; configure and generate completed.
2. `cmake --build build-linux`: exit 0; all targets built.
3. `ctest --test-dir build-linux --output-on-failure` in the restricted sandbox:
   exit 8, **18 executed, 4 passed, 14 failed**, 0.07 seconds. All cases requiring
   AF_INET socket creation failed because the sandbox returned EPERM (`Operation
   not permitted`). This was an environment restriction, not a code defect.
4. The identical CTest command with permission outside the socket restriction:
   exit 0, **18 executed, 18 passed, 0 failed**, 0.06 seconds.

The nine portable TCP cases passed: listener, establishment, independence,
connection_moves, listener_moves, listener_destruction, refusal, bind_failure,
and invalid_address. Their 20-second CTest timeouts remain in force. The Windows
`no_runtime` case remains Windows-only. Linux byte-transport and framing-over-TCP
tests were not enabled.

## Evidence boundary

Implemented and Linux-tested: listener creation, ephemeral ports, loopback
connection, independent accepted ownership, connection/listener moves and
destruction, invalid IPv4 rejection, refused connection, duplicate active bind
rejection, and POSIX native-error propagation.

Not tested: Release configuration, Windows behavior in this session, external
networks, IPv6, DNS, injected `EINTR`, resource exhaustion, or post-close rebinding.
The next recommended increment is POSIX `TcpConnection::sendAll` and `receive`
with focused Linux byte-transport tests.
