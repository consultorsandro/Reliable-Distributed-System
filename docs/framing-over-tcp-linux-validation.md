# Linux/POSIX framing-over-TCP validation — 2026-10-17

Working directory: `/home/veterano_linux/Reliable-Distributed-System`.

## Objective and scope

This increment enables and validates the existing Step 3.4-B4 framing-over-TCP tests
on Linux/POSIX. It composes the existing `FrameEncoder`, `FrameStreamDecoder`, and
`Message` contract with the existing blocking `TcpListener`, `TcpConnection::sendAll`,
and `TcpConnection::receive` transport APIs. No application protocol, production socket
code, public API, ownership rule, or library boundary changed.

The tests preserve TCP byte-stream semantics: every positive native receive result is
passed to the persistent stream decoder, without treating send calls, receive calls,
frames, or packets as equivalent boundaries. `finish()` is used at actual orderly EOF
or in the explicit finalization-contract case.

## Files and implementation boundary

Modified:

- `tests/CMakeLists.txt`: removed only the Windows gate around the existing executable
  and ten CTest registrations; retained MSVC `/EHsc` and 20-second test timeouts.
- `docs/framing-over-tcp.md`: records cross-platform enablement and separates the Linux
  and Windows evidence boundaries.

Created: `docs/framing-over-tcp-linux-validation.md`. No file was deleted or renamed.
No production source or public header was modified.

The existing components were reused unchanged: protocol encoding/decoding and message
types from `rds_protocol`; POSIX connection establishment and byte transport from
`src/transport/tcp_posix.cpp`; POSIX socket ownership from
`src/transport/socket_handle_posix.cpp`; and the no-op compatibility runtime from
`src/transport/winsock_runtime_posix.cpp`. Only the integration test target links both
`rds_protocol` and `rds_transport`, preserving their production separation.

The Windows-specific files affected by the build selection but not modified are:

- `/home/veterano_linux/Reliable-Distributed-System/src/transport/tcp.cpp`: Winsock
  connection establishment and blocking byte transport.
- `/home/veterano_linux/Reliable-Distributed-System/src/transport/socket_handle.cpp`:
  exclusive `SOCKET` ownership and `closesocket` cleanup.
- `/home/veterano_linux/Reliable-Distributed-System/src/transport/winsock_runtime.cpp`:
  `WSAStartup`/`WSACleanup` lifecycle.

Windows test source, target behavior, MSVC settings, file names, and test names remain
unchanged. `docs/framing-over-tcp-validation.md` remains the preserved historical
Windows/MSVC result. Windows was not rebuilt in this Linux session.

## Test cases and results

The enabled cases are `finish_contract`, `binary`, `one_byte`, `multiple`,
`complete_partial`, `maximum`, `bidirectional`, `complete_eof`, `truncated_eof`, and
`malformed`. Together they cover binary payload preservation; arbitrary bounded
chunking down to one byte; ordered multiple frames on one connection; a complete frame
followed by a partial frame; the 65,535-byte maximum payload and 65,540-byte frame;
two-way framed transport; clean EOF at a frame boundary; rejection of every nonempty
proper prefix at EOF; malformed length/type rejection; and the non-mutating, repeatable
`FrameStreamDecoder::finish()` contract.

Commands:

```text
cmake -S . -B build-linux
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

Configure: exit 0; configuration and generation completed. Build: exit 0; all targets,
including `framing_tcp_tests`, built successfully.

The first CTest run in the restricted sandbox exited 8: **37 executed, 5 passed,
32 failed**, 0.13 seconds. All 32 failures required native AF_INET socket creation and
reported `EPERM` (`Operation not permitted`), directly or through a test helper. The
five non-socket paths passed, including `framing_tcp_finish_contract`. This was an
execution-environment restriction, not a production or test defect; production socket
code was not changed to bypass it.

The identical CTest command was rerun with native socket permission: exit 0,
**37 executed, 37 passed, 0 failed**, 2.55 seconds. All ten framing-over-TCP cases passed
with their existing behavioral assertions and 20-second per-case timeouts. The maximum
frame case was the longest at 2.43 seconds. No implementation or test defect was found.

## Validated guarantees and limitations

Successful execution validates same-process Linux loopback composition: socket
ownership, connection establishment, binary byte transport, frame reconstruction under
the tested arbitrary chunking, message order and content, bidirectional flow, framing
limits, malformed-input rejection, and clean-versus-truncated EOF distinction.

It does not validate independent application processes, Client/Server behavior,
application PING/PONG handling, acknowledgement, persistence, exactly-once effects,
retries, reconnection, production timeouts, asynchronous I/O, external networks, IPv6,
TLS, authentication, deterministic packet boundaries, induced short sends, throughput,
resource exhaustion, Release configuration, or Windows behavior in this session.
Caller-selected receive capacities demonstrate decoder chunk independence, not network
packetization. CTest timeouts bound the test harness only.

Linux evidence uses the POSIX implementation and native Linux loopback sockets. Windows
evidence is the earlier MSVC/WinSock run in `framing-over-tcp-validation.md`; its 38/38
result is historical and was neither replaced nor revalidated here. The total counts
differ because Windows registers its runtime-lifecycle and missing-runtime cases, while
Linux registers the POSIX-only SIGPIPE-suppression case.

## Next recommended step

Phase 3 — Step 3.4-B5: First End-to-End PING/PONG Exchange using independent Client and
Server application processes rather than only same-process integration tests.
