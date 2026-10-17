# TCP connection establishment (Step 3.4-B2)

Phase 3 remains in progress. `rds_transport` establishes synchronous TCP
connections on Windows and Linux without depending on protocol or framing.

## Linux/POSIX extension

`src/transport/tcp_posix.cpp` implements listener creation, ephemeral-port
discovery, blocking accept, and blocking connect with `socket`, `bind`, `listen`,
`getsockname`, `accept`, `connect`, and `inet_pton`. It uses `socklen_t` and captures
`errno` immediately after native failures, throwing `std::system_error` with the
system category. Numeric IPv4 validation continues to throw `std::invalid_argument`.
POSIX `accept` and `connect` surface `EINTR` rather than retrying automatically;
the public API has no implicit interruption, retry, or timeout policy.

Windows remains implemented by `src/transport/tcp.cpp`, including
`SO_EXCLUSIVEADDRUSE`, establishment, and the existing byte operations.
`src/transport/socket_handle.cpp` owns Windows `SOCKET` cleanup with `closesocket`,
and `src/transport/winsock_runtime.cpp` owns `WSAStartup`/`WSACleanup`. CMake selects
those files and `ws2_32` only on Windows. The Linux equivalents are
`src/transport/socket_handle_posix.cpp` and the no-op compatibility runtime in
`src/transport/winsock_runtime_posix.cpp`.

Linux deliberately uses normal `bind` behavior and enables neither `SO_REUSEADDR`
nor `SO_REUSEPORT`. A duplicate active listener on the same address and port is
tested and rejected. This is not equivalent to Windows `SO_EXCLUSIVEADDRUSE`:
platform exclusivity guarantees differ, and TCP state such as `TIME_WAIT`, address
specificity, and kernel policy can limit or delay port rebinding after closure.

The public signatures and move-only RAII ownership contracts are unchanged.
Linux byte transport and framing-over-TCP remain outside this increment and their
tests remain disabled on Linux. See
[Linux validation](tcp-establishment-linux-validation.md); the historical
[Windows validation](tcp-establishment-validation.md) is preserved.

## API and ownership

- `TcpListener(address, port)` creates, exclusively binds, and listens on one
  IPv4 stream socket. Port zero requests an OS-assigned port; `localPort()` queries
  it with `getsockname`. Windows `SO_EXCLUSIVEADDRUSE` prevents address takeover.
- `connectTcp(address, port)` blocks until native connection establishment succeeds
  or fails, returning a `TcpConnection` on success.
- `TcpListener::accept()` blocks and returns a connection owning the newly accepted
  socket. The listener retains its separate listening socket.
- `TcpConnection` exclusively owns one established socket. Its private constructor
  accepts a moved `SocketHandle`; only the listener and connector can construct it.
  Arbitrary public adoption and default construction are unavailable.
- Both classes are noncopyable with nonthrowing moves. SocketHandle performs all
  socket cleanup, including replacement on assignment and exception unwinding.
  A moved-from connection exposes `INVALID_SOCKET` through borrowed `nativeHandle()`;
  callers must not close, adopt, or change settings through this borrowed handle.
  Moved-from listener operations throw native invalid-socket errors.
- The caller must declare `WinsockRuntime` before the owners and keep it alive
  through all socket operations and destruction. B1 lifetime management is unchanged.

Addresses are numeric IPv4 only, with no DNS or IPv6. Bounded copying supplies a
terminated buffer to `InetPtonA`; empty, oversized, embedded-NUL, and malformed
addresses throw `std::invalid_argument`. Native failures throw `std::system_error`
with the operation, native code, and system message. This covers socket creation,
exclusive binding configuration, bind, listen, accept, connect, and port discovery.
No retry, timeout policy, production threads, byte I/O, or application integration
is implemented. Native blocking durations remain OS-controlled.

## Executable evidence design

Eleven new static assertions cover noncopyability, nonthrowing moves and destruction
of both owners, plus the private connection construction boundary. These are not
runtime test cases.

Ten new CTest cases use separate processes and a 20-second harness timeout:

| Group | Cases | Evidence |
| --- | --- | --- |
| Loopback integration | listener, establishment, independence, connection_moves, listener_moves, listener_destruction | Ephemeral binding; native connected peers on both sides; accepted-socket closure followed by listener reuse; move transfer and replacement closure; listener moves; accepted socket survives listener destruction |
| Local native failure integration | refusal, bind_failure | Refused connection to an exclusively reserved non-listening port; duplicate exclusive bind rejection |
| Component validation | invalid_address, no_runtime | Invalid inputs rejected by both creation APIs; socket creation reports missing Winsock initialization |

Listener move tests also check controlled failures of `accept` and `getsockname`
on the emptied source. Connected peers are verified through `getpeername`, without
sending or receiving bytes. Closed handles are probed before another socket is
allocated, avoiding handle reuse within these single-threaded test processes.
Tests connect before accept using the listening backlog; no helper threads or sleeps
are needed. The CTest timeout bounds a hung test process, not production networking.

The refusal test retains an exclusive bound socket without calling `listen`, avoiding
an unused-port close/rebind race. The observed refusal still depends on the Windows
network stack and local filtering policy; it is not a universal environment guarantee.
Failed construction cleanup relies on the unchanged, separately tested SocketHandle
RAII; no exhaustive leak instrumentation or fault injection is claimed.

## Limits and next increment

Loopback establishment does not establish Internet connectivity, remote host behavior,
packet-loss tolerance, timeout or retry behavior, partial sends or receives,
application-level delivery, framing over TCP, PING/PONG over TCP, TLS security,
fault tolerance, or production concurrency behavior. Release builds, non-Windows
platforms, IPv6, and DNS are not validated. Listen failure, setsockopt failure,
and resource-exhaustion failure are handled but not fault-injected.

Next: **Step 3.4-B3 — TCP Byte Transport**, focused on the previously defined
`sendAll()` and `receive()` contracts. B3 is not implemented here.

The original text below describes the historical Windows B2 increment. See
[Windows session validation](tcp-establishment-validation.md) for its commands and results.

## Subsequent byte-transport increment

Step 3.4-B3 adds `sendAll()` and `receive()` to TcpConnection. See
[TCP byte transport](tcp-byte-transport.md) for current byte API contracts and
validation. The preceding no-byte-I/O statements describe B2's historical scope.
