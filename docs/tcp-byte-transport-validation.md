# Step 3.4-B3 validation — 2026-09-16

## Implementation and files

Working directory: `C:\Users\Meu Computador\Documents\Reliable-Distributed-System`.
Build: Windows/MSVC, Visual Studio 18 2026, C++20, Debug, SDK 10.0.26100.0.
Scope: blocking binary byte I/O over established TCP connections. No framing integration.

Modified:
- `include/rds/transport/tcp_connection.hpp`: public byte API and contracts.
- `src/transport/tcp.cpp`: bounded partial-send loop and single native receive.
- `tests/CMakeLists.txt`: eight new independent cases, 20-second harness timeouts.
- `docs/tcp-establishment.md`: link to B3 while retaining historical B2 evidence.

Created:
- `tests/tcp_byte_transport_tests.cpp`
- `docs/tcp-byte-transport.md`
- `docs/tcp-byte-transport-validation.md`

SocketHandle, WinsockRuntime, listener/connector logic, protocol sources, root CMake,
and all previous test cases/assertions remain unchanged. No commit was requested.

## Exact validation workflow

The known B2 duplicate Path/PATH environment issue was avoided proactively with
process-local normalization. This does not modify the user or machine environment.
Exact PowerShell build invocation:

```powershell
$buildPathValue = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $buildPathValue
cmake --build build --config Debug
```

Build completed with exit 0. CMake automatically regenerated because
`tests/CMakeLists.txt` changed. No explicit configure command was needed.
Only after build completion:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Result: exit 0, **28 executed, 28 passed, 0 failed**, 3.81 seconds reported by CTest.
All 20 previous cases and all 8 new cases passed. No unexpected build/test failures,
corrections, or retests occurred in this B3 session. Expected exception cases are
successful tests, not regressions. The earlier B2 failure history remains in its own
validation report and was not erased or reclassified.

## Evidence classification

- Compile-time: 21 existing ownership assertions preserved; no new assertions added.
  B2 assertions recompiled after the connection header changed; B1 target remained
  up to date. Assertions are not included in the 28 CTest runtime entries.
- Component/API runtime: empty send, empty receive rejection, and moved-from behavior.
  Tests also prove subsequent byte transfer for valid owners after local rejection.
- Real loopback: binary bytes including 00/01/7F/80/FF preserved, same connection pair
  transfers both ways, multiple sequential sends reconstruct one ordered stream,
  repeated bounded reads reconstruct the larger payload, and graceful closure returns
  zero after all expected bytes have been consumed.
- Failure path: nonempty send and receive on a moved-from owner each retain the native
  error category, operation name, and WSAENOTSOCK (10038); moved-to owner still transfers.

Relevant executable output:

```text
Reconstructed 4096 binary bytes using 133 reads (capacity 31)
Expected send failure: send: An operation was attempted on something that is not a socket. [10038]
Expected recv failure: recv: An operation was attempted on something that is not a socket. [10038]
100% tests passed, 0 tests failed out of 28
```

## Important unexercised paths and interpretation

Actual positive-but-short native send was not instrumented or observed. The reviewed
loop handles that result, but loopback success alone cannot establish its execution.
No mocking seam was added. Zero-progress send, send failure after partial acceptance,
spans above INT_MAX, reset/abort while transferring, native exhaustion, and cleanup
failure were not fault-injected. Multiple reads were forced by caller capacity;
no spontaneous fragmentation or packet-boundary claim is made.

No Release, external-network, non-Windows, performance, security, timeout, retry,
framing-over-TCP, or application-delivery validation was performed. Ownership remains
safe for destruction/move after error, but peer liveness and recovery are not promised.
See `tcp-byte-transport.md` for complete API and uncertain-partial-effect semantics.

Next increment: integrate the validated framing layer with byte transport; it is not
implemented in this session. Phase 3 remains in progress.

Step 3.4-B3 — implemented and validated within its defined scope.
