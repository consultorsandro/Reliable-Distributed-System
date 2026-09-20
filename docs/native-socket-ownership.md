# Native socket ownership foundation (Step 3.4-B1)

The Windows-only `rds_transport` CMake library links Winsock (`ws2_32`) and has
no dependency on the protocol or framing library. Existing framing behavior and
tests are unchanged. Phase 3 remains in progress.

## Implemented behavior and lifetime contract

`rds::transport::SocketHandle` adopts exclusive ownership of a `SOCKET` through
an explicit, nonthrowing constructor. Default construction and adoption of
`INVALID_SOCKET` produce an empty owner. Destruction calls `closesocket` for a
valid handle. Moves empty the source; move assignment closes the previous
destination first. Self move is harmless. `nativeHandle()` provides borrowed
access for future transport operations; it does not transfer ownership. No
release, reset, connection, listening, or byte I/O API is provided.

The caller must supply a live, uniquely owned socket or `INVALID_SOCKET`.
Duplicate adoption, external closure, and use after runtime destruction violate
the API contract; these conditions cannot be reliably detected by this wrapper.
Preserve the default disabled `SO_LINGER` setting. In particular, nonblocking
sockets with enabled positive linger can make `closesocket` fail without releasing
the socket. This increment does not configure or support that cleanup policy.

`WinsockRuntime` requests Winsock 2.2. Each instance balances one successful
`WSAStartup` with `WSACleanup`, using Winsock's native reference counting. The
runtime is neither copyable nor movable. Declare it before socket owners and
keep it alive until all owners and socket operations have finished. This lifetime
relationship is a caller obligation, not enforced by the type system.

Initialization errors throw `std::system_error` naming the operation and retaining
the returned native error code. An unsupported negotiated version cleans up the
successful startup before throwing. Cleanup errors are reported to standard error
with the operation and native code: destructors and moves remain `noexcept`.
Cleanup failures are not retried and successful release cannot be guaranteed if
the operating system rejects cleanup. There is no singleton or custom global state.

## Verification boundaries

Ten `static_assert` checks verify noncopyability of both types, nonthrowing socket
moves, nonmovability of the runtime, and nonthrowing destruction of both types.
These compile-time checks are not CTest runtime cases.

Eight independent CTest cases cover empty destruction without initialization,
acquisition and destruction, move construction, assignment over an owned socket,
empty moves, self move, exception unwinding, and nested/repeated runtime lifetimes.
Real unconnected stream sockets are probed with `getsockopt(SO_TYPE)` for usability
and `WSAENOTSOCK` after cleanup. No intervening socket allocation occurs between
release and a closed-handle probe. Runtime lifecycle tests also observe
`WSANOTINITIALISED` after the final runtime leaves scope, in isolated test processes.
There are no ports, remote servers, external networks, sleeps, or ordering dependencies.

These observations establish public ownership states and closure of the tested
resources. They do not instrument exact close-call counts or prove absence of
leaks under every OS failure. Startup failure, version mismatch, and cleanup
failure paths are not fault-injected. Release builds and non-Windows platforms
are not covered by this increment's Debug validation.

## Build and future work

Use the existing Windows/MSVC build directory:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CTest registers the two existing protocol/framing suites plus eight ownership
cases on Windows. Non-Windows configuration does not create transport targets.

The next increment can define synchronous connection establishment and the
construction/lifetime contracts of `TcpConnection` and `TcpListener`, using these
owners. Neither those types nor TCP communication are implemented here.

## Subsequent increment

Step 3.4-B2 adds connection establishment on top of this unchanged ownership
foundation. See [TCP connection establishment](tcp-establishment.md) for the
current transport API and validation boundaries. The statements above describe
B1's original scope.
