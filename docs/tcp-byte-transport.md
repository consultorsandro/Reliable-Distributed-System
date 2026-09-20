# TCP byte transport — Step 3.4-B3

Phase 3 remains in progress. TcpConnection now provides blocking binary byte I/O.
SocketHandle ownership, WinsockRuntime lifetime, and B2 establishment remain intact.
The transport library has no protocol/framing dependency or message-size policy.

## API contracts

`void sendAll(std::span<const std::byte> data)` submits bytes until all have been
accepted by successful local socket send operations or an error occurs. Empty input
is a no-op, even on a moved-from owner, with no native call. Each native length is
bounded by INT_MAX before conversion to int. Positive send results advance the span
by the actual count, preserving the remaining suffix across short successful sends.
SOCKET_ERROR throws std::system_error with the native code, operation, and message.
A zero result for nonempty input throws std::runtime_error instead of looping without
progress; there is no fresh native error code to associate with that result.

Normal return does not establish peer application consumption, processing, persistence,
a successful application operation, or a future response. Failure may follow acceptance
of a prefix. The API returns no partial count and cannot roll back accepted bytes.
The caller must never infer zero bytes sent from an exception or blindly replay the
whole input. Automatic retry and application deduplication are outside B3.

`[[nodiscard]] std::size_t receive(std::span<std::byte> buffer)` performs one native
recv with capacity bounded by INT_MAX. It does not fill the buffer or preserve send
boundaries. A positive count identifies bytes written at the start; zero means orderly
EOF after queued stream data is exhausted; SOCKET_ERROR throws std::system_error.
Empty destinations throw std::invalid_argument before any socket operation, preserving
zero exclusively for EOF. A nonempty operation on a moved-from connection reports
WSAENOTSOCK. No connection-liveness accessor is added.

Both operations are synchronous, without timeout policy, retries, polling, production
threads, or asynchronous I/O. The caller must keep WinsockRuntime alive and supply
valid spans. Binary values, including NUL and high-bit bytes, have no text semantics.

## Failure and remaining state

Send/receive may fail because the socket is invalid, its state disallows the operation,
the connection is reset/aborted, or the OS encounters network/resource errors. Native
errors are captured immediately. Only invalid-socket byte failures are induced by this
suite; remote failure timing is not asserted. Peer destruction alone does not establish
that the next local send must fail.

Errors do not release or replace the owned socket; RAII destruction and moves remain
usable. A retained native handle is not evidence of a healthy peer or a reusable stream.
After a native failure, further communication depends on the error and OS state; this
API promises no recovery. A local empty-buffer rejection does not consume data or
change the connection, and the tests demonstrate subsequent normal transfer. Calls on
a moved-from source do not damage the moved-to owner's established connection.

## Test design and evidence boundaries

Eight new individually registered CTest cases supplement the unchanged 20 previous
cases. Each process initializes Winsock before socket owners. A test-only Pair helper
uses loopback and port zero, connecting through the backlog before accept. Payloads
are bounded (at most 4096 bytes) and sent before receive; no helper threads or sleeps
are needed for these modest transfers in the tested environment. Each new CTest has
a 20-second process timeout to bound hangs, not a production transport timeout.

| Case | Property |
| --- | --- |
| empty_send | Empty input returns normally and inserts no stream bytes |
| empty_receive | Empty destination throws invalid_argument; connection remains usable |
| binary | Exact preservation of 00 01 7F 80 FF, using a buffer larger than available data while peer stays open |
| bidirectional | The same established pair carries distinct binary sequences both ways |
| repeated_reads | Exact reconstruction of 4096 bytes covering all 256 values with a 31-byte buffer |
| sequential_sends | Three sends are reconstructed as one ordered stream, without assumed call correspondence |
| orderly_closure | All expected bytes are consumed before peer destruction; subsequent receive returns zero |
| moved_from | Both nonempty byte operations report native invalid-socket failures; moved-to connection remains usable |

The collection helper checks positive counts, capacity bounds, exact stream content,
untouched destination suffix, and sentinel bytes outside the supplied span. It tolerates
arbitrary positive read sizes instead of assuming one send equals one receive.

Existing 21 ownership static assertions remain. No new static assertions are added:
the byte API's important new contracts are dynamic and covered by runtime tests.

## Partial send decision

The production loop was reviewed for capped native lengths, immediate error capture,
zero-progress termination, actual-count advancement, and bounded remaining lengths.
No actual positive-but-short native Winsock send result was instrumented or observed.
Loopback success is not proof of that branch. Deterministically forcing a short native
send on blocking sockets would require more intrusive controls or timing assumptions;
a new mock seam was not justified for this small increment. No synthetic coverage is
claimed. Zero-progress sends, failure after partial acceptance, and spans above INT_MAX
were also not dynamically exercised.

The repeated-read test forces multiple reads through destination capacity. It does
not establish spontaneous TCP fragmentation, packet boundaries, or partial native send
behavior. The large payload test is neither a benchmark nor an exhaustion test.

## Unresolved scope

No external network, remote-failure recovery, packet-loss behavior, production throughput,
Release build, or non-Windows platform was validated. Framing, Message transmission,
PING/PONG, application delivery/acknowledgement, retry/deduplication, persistence,
timeouts, reconnect, security, and distributed fault tolerance remain outside scope.
Next increment: integrate the existing validated framing layer with byte transport.

See [session validation](tcp-byte-transport-validation.md) for executable results.

## Subsequent framing integration

Step 3.4-B4 composes the byte API with framing in tests without changing transport.
See [framing over TCP](framing-over-tcp.md), including explicit EOF validation.
The preceding integration exclusions describe B3's historical scope.
