# Step 3.4-B4 validation — 2026-09-16

Working directory: `C:\Users\Meu Computador\Documents\Reliable-Distributed-System`.
Configuration: Windows, MSVC / Visual Studio 18 2026, C++20, Debug,
Windows SDK 10.0.26100.0 targeting Windows 10.0.19045.

## Files

Modified: `include/rds/protocol/frame_stream_decoder.hpp`,
`src/protocol/frame_stream_decoder.cpp`, `tests/CMakeLists.txt`,
`docs/tcp-byte-transport.md`.
Created: `tests/framing_tcp_tests.cpp`, `docs/framing-over-tcp.md`,
`docs/framing-over-tcp-validation.md`.

Only production change: const finish() framing-boundary validation. Before B4,
consume/reset could not report pending partial state at EOF. finish returns normally
when no incomplete bytes remain and throws invalid_argument otherwise, without
mutation. Existing consume and transport semantics are unchanged. No new production
wrapper or application PING/PONG logic was added. No commit was requested.

## Exact commands

Process-local environment normalization proactively retains the B2 workaround for
duplicate Path/PATH keys; user/machine environment settings are not changed.

```powershell
$buildPathValue = $env:Path
Remove-Item Env:PATH -ErrorAction SilentlyContinue
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $buildPathValue
cmake --build build --config Debug
```

Build: exit 0. CMake automatically regenerated due to tests/CMakeLists.txt changes;
no explicit configure command. After successful build completion:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Result: exit 0, **38 executed, 38 passed, 0 failed**, 3.00 seconds.
No build/test overlap. No unexpected intermediate failures, corrective changes,
or retests in B4. Expected framing exceptions are successful negative tests.

## Evidence classes

- Compile time: all 22 existing assertions preserved (21 ownership checks plus the
  framing size-overflow bound); none added. They are excluded from runtime totals.
- Existing unit/component: the two framing suite entries and eight native ownership
  entries remain enabled and passing, with unchanged assertions.
- Existing TCP: ten establishment and eight byte transport entries remain passing,
  including native failures separately reported as system_error.
- New component: finish_contract validates empty/complete EOF, every proper partial
  split, unchanged state after rejection, repeated calls, and explicit reset.
- New loopback integration: nine cases exercise binary, one-byte reads, sequential
  frames, complete plus partial next frame, maximum frame, both directions, complete
  EOF, truncated EOF, and malformed input. Ports are OS-assigned, loopback only.

Observed excerpts:

```text
10 frame bytes, 4 reads
10 frame bytes, 10 reads
65540 frame bytes reconstructed with 513 bounded reads
Expected framing rejection: Incomplete frame at end of input
Expected framing rejection: Message body must contain a message type
Expected framing rejection: Declared message body exceeds maximum size
Expected framing rejection: Unknown message type
100% tests passed, 0 tests failed out of 38
```

The 65535-byte maximum payload contains all binary byte values. Header and body
truncation were exercised at all nine nonempty proper prefixes of the ten-byte sample
frame. No Message was emitted; transport EOF remained zero, then finish rejected.
The complete-frame counterpart retained the Message after EOF and passed finish.

## Reliability and limitations

See `framing-over-tcp.md` for per-case properties, nonclaims, EOF and reliability
contracts. The EOF gap is resolved for callers that invoke finish before resetting
state. No automatic EOF detection inside framing, recovery, or resynchronization is
introduced. Malformed tests supply explicitly delimited invalid inputs and check the
existing cleared state; this is not arbitrary-stream recovery.

Read capacities/test phase budgets force multiple receives. No spontaneous packet
fragmentation or specific native coalescing is asserted. Maximum frame transfer uses
bounded alternating send/read phases, not a single maximum-sized native send. Actual
short native send, failure after partial acceptance, reset mid-frame, external hosts,
Release, non-Windows, throughput, exhaustion, TLS, retry, acknowledgements, persistence,
or application end-to-end behavior were not validated. Failed frame send can have
uncertain partial effect; automatic resend remains unsafe and absent.

Next: Step 3.4-B5 — First End-to-End PING/PONG Exchange. Phase 3 remains in progress.

Step 3.4-B4 — implemented and validated within its defined scope.
