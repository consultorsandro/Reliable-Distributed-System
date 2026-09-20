# Step 3.4-B2 validation session — 2026-09-16

## Scope and files

Working directory: `C:\Users\Meu Computador\Documents\Reliable-Distributed-System`.
Configuration: Windows, C++20, MSVC / Visual Studio 18 2026, Debug;
Windows SDK 10.0.26100.0 targeting Windows 10.0.19045.

Created:
- `include/rds/transport/tcp_connection.hpp`
- `include/rds/transport/tcp_listener.hpp`
- `src/transport/tcp.cpp`
- `tests/tcp_establishment_tests.cpp`
- `docs/tcp-establishment.md`
- `docs/tcp-establishment-validation.md`

Modified: root `CMakeLists.txt`, `tests/CMakeLists.txt`, and
`docs/native-socket-ownership.md`. Existing source and test cases are preserved.
The initial working tree already contained untracked project files; no commit or
index changes were made. Scope is connection establishment only, not byte transport.

## Commands and intermediate failures

1. `cmake --build build --config Debug`

   CMake automatically regenerated after CMakeLists.txt changed; no explicit
   reconfiguration command was needed. Build failed with environmental MSB6001:
   `CL.exe ... System.ArgumentException ... 'Path' ... 'PATH'` (duplicate environment
   dictionary key). Compilation could not launch. CTest was not run after this failure.
   The source of the inherited duplicate capitalization was not established.

2. Rebuild with process-local environment normalization, exact PowerShell:

   ```powershell
   $buildPathValue = $env:Path
   Remove-Item Env:PATH -ErrorAction SilentlyContinue
   Remove-Item Env:Path -ErrorAction SilentlyContinue
   $env:Path = $buildPathValue
   cmake --build build --config Debug
   ```

   Result: exit 0, including the new TCP executable and compile-time checks.
   No machine/user environment settings were changed.

3. After build completion: `ctest --test-dir build -C Debug --output-on-failure`

   Result: 20 executed, 19 passed, 1 failed; 11.54 seconds reported by CTest.
   Test assertion failure in `tcp_bind_failure`:
   `Expected failure: bind: Only one usage of each socket address
   (protocol/network address/port) is normally permitted. [10048]`
   followed by `Unexpected native error code`.
   Diagnosis: the implementation correctly rejected the duplicate bind; the test
   expected WSAEACCES instead of WSAEADDRINUSE for this exclusive-bind configuration.
   Correction: expected code changed to WSAEADDRINUSE (10048). No production change.
   All ten previous CTest cases and the other nine new cases passed on this run.

4. Rebuild using the exact normalized-environment command block above, then rerun
   `ctest --test-dir build -C Debug --output-on-failure` only after build exit 0.

## Evidence classification and limits

Eleven added static assertions compile separately from runtime totals: both owners
are noncopyable, nonthrowing movable and destructible; public connection adoption
is prohibited. The previous ten static assertions remain unchanged.

CTest registers executable cases directly (no GoogleTest discovery): two existing
framing suite entries, eight existing ownership cases, and ten new TCP cases.
The ten new cases comprise eight local loopback/native integration cases and two
component cases. The framing entries each contain their existing internal checks;
they are not expanded into extra CTest counts.

Expected native errors exercised: connect refusal 10061, duplicate bind 10048,
accept/getsockname on moved-from listener 10038, and socket creation without runtime
10093. Invalid addresses are separately rejected as std::invalid_argument.
The reserved-port refusal design and its environmental limitations are described
in `tcp-establishment.md`. Native error messages are emitted by the test helper.

Not executed: Release, external network, non-Windows, fault injection for listen,
setsockopt, resource exhaustion or cleanup; exhaustive leak analysis. Loopback
results do not prove Internet connectivity, remote host behavior, packet-loss
tolerance, timeout or retry behavior, partial send/receive handling, application
delivery, framing, PING/PONG networking, TLS security, fault tolerance, or production
concurrency. No byte transport API was implemented or tested.

## Final validation result

The corrected Debug rebuild completed with exit 0. The subsequent full CTest run
completed with exit 0: **20 executed, 20 passed, 0 failed**, 2.79 seconds.
All ten prior runtime entries and all ten new entries passed. The eleven new
compile-time assertions compiled successfully and are excluded from that total.
There were no remaining build or test failures. No CTest run overlapped a build.

Observed properties include real loopback connection and acceptance, retained
listener usability after accepted-socket destruction, accepted connection ownership
after listener destruction, connection moves and replacement closure, and controlled
local failure reporting. This establishes connection creation within the stated
scope, not reliability under arbitrary network failures.

Classification: **implemented and validated within its defined scope**.
Next increment: **Step 3.4-B3 — TCP Byte Transport** (`sendAll()` and `receive()`).
