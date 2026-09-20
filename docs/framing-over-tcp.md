# Framing over TCP — Step 3.4-B4

Phase 3 remains in progress. Composition is implemented in integration tests:

Message -> FrameEncoder -> TcpConnection::sendAll -> TCP byte stream ->
TcpConnection::receive -> FrameStreamDecoder::consume -> Message.

Only the integration test target links both libraries. Transport remains byte-only;
framing remains socket-independent. No new connection wrapper or Client/Server
application behavior was introduced. Protocol v0.1 remains a four-byte big-endian
body length, one-byte type (PING 01, PONG 02), and binary payload. Limits remain
65536 body bytes, 65535 payload bytes, and 65540 complete frame bytes.

## Minimal production API change: end-of-input validation

B4 discovered that consume/reset could not distinguish clean EOF between frames
from EOF with buffered partial framing. Empty consume does not validate completion.
Added `void FrameStreamDecoder::finish() const`: returns normally exactly when no
incomplete frame bytes remain; otherwise throws std::invalid_argument with
`Incomplete frame at end of input`. It neither changes buffered state nor seals the
decoder. Repeated validation is allowed; reset remains an explicit discard operation.
Existing consume behavior is unchanged. No socket knowledge is introduced.

The integration caller invokes finish after receive returns zero. Transport zero
means orderly end of the byte stream, not a complete protocol stream. With no buffered
partial frame this is a clean framing boundary; with partial bytes it is truncation.
No recovery is attempted after EOF. A const finish is also usable for validating a
known input boundary in tests, but callers must not reset before EOF validation and
thereby hide truncation. Successful finish is not a certificate that earlier malformed
input never occurred: consume errors must still be handled separately.

## Error and reliability semantics

Receive chunks are not protocol units and TCP does not preserve write/frame boundaries.
The same decoder retains incomplete length/body bytes across calls and validates input
before emitting owned Messages. Network input is never decoded by bypassing validation.
Native send/receive errors remain std::system_error; malformed lengths/types and
truncation are framing std::invalid_argument. Malformed consume clears buffered state,
returns no messages from that failed call, and discards its remaining input. There is
no automatic resynchronization, retry, acknowledgement, or application delivery claim.

Encoding can reject invalid messages or fail allocation; send can fail before all frame
bytes are accepted. A failed sendAll may already have accepted a prefix. Resending is
unsafe without a later protocol policy because it may duplicate bytes or break framing.
TCP closure between frame bytes leaves incomplete decoder state, rejected by finish.
A complete decoded Message establishes validated type/payload reconstruction for the
observed stream, not peer application processing, persistence, exactly-once effects,
or a response. Already returned Messages own their payload and survive connection EOF.

## Executable evidence

Ten new CTest entries: one component test and nine real loopback integration cases.
All previous 28 runtime entries remain unchanged. Existing static assertions are
preserved; no new static assertions are needed for the dynamic EOF contract.

| Case | Observed property | Limit |
| --- | --- | --- |
| finish_contract | Empty/complete boundaries pass; every partial split rejects; validation preserves state; reset discards explicitly | In-memory contract, not socket evidence |
| binary | Exact binary type/payload through 3-byte receive buffers | Caller-forced reads, not packet fragmentation |
| one_byte | Real receive feeds one byte at a time into persistent decoder | Not a performance result |
| multiple | Three separately sent frames reconstruct in order | Native coalescing is allowed, not required or asserted |
| complete_partial | A emitted, six bytes of B retained, B emitted only after remainder | Controlled send/read phase, not a guaranteed single native chunk |
| maximum | 65540-byte frame / 65535-byte binary payload reconstructed exactly | Bounded integration, not throughput or exhaustion |
| bidirectional | Different framed messages decoded both ways on one pair | No application PING/PONG processing |
| complete_eof | Complete message remains valid after transport zero and finish success | Orderly local closure only |
| truncated_eof | Every nonempty proper prefix of a small frame emits nothing and rejects at EOF | No remote reset/recovery evidence |
| malformed | Zero length, oversize length, and unknown type cross TCP then throw framing errors; buffered state cleared | No automatic stream repair |

The byte-budget helper feeds exactly each positive receive result to consume and
collects all emitted messages; it never assumes send/receive boundary correspondence.
All ports are OS-assigned on 127.0.0.1. Maximum-frame transfer alternates bounded 1024-byte
send phases with 137-byte receive buffers, avoiding dependence on queuing the entire
frame before reading. No threads, detached work, sleeps, or timing assumptions are used.
CTest's 20-second harness timeout bounds hangs without adding production timeouts.

## Unexercised paths and next step

No actual short native send, arbitrary remote failures, packet-loss recovery, TLS,
authentication, Internet connectivity, application acknowledgement, retry/deduplication,
timeout, performance, Release, or non-Windows validation is claimed. Exact chunk/packet
boundaries are deliberately unspecified. Protocol/framing and byte-transport guarantees
remain separate from application reliability.

Next: Step 3.4-B5 — First End-to-End PING/PONG Exchange across real Client and Server
processes. B5 is not implemented here.
