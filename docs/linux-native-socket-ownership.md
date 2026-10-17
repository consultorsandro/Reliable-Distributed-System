# Linux native socket ownership foundation

This portability increment makes the native socket ownership foundation
available on Linux without implementing Linux connection establishment or byte
transport. The higher-level `TcpConnection` and `TcpListener` architecture is
unchanged, but their native operations remain Windows-only at this stage.

## Preserved Windows implementation

The original Windows sources remain in place:

- `src/transport/socket_handle.cpp` implements exclusive `SOCKET` ownership,
  move transfer, and cleanup through `closesocket`, reporting `WSAGetLastError`.
- `src/transport/winsock_runtime.cpp` balances `WSAStartup(2.2)` with
  `WSACleanup` and validates the negotiated Winsock version.
- `src/transport/tcp.cpp` implements the Winsock listener, connection, send,
  and receive operations. It is not replaced by this increment.

Windows continues to link `ws2_32`. The historical Windows validation in
`native-socket-ownership.md` and the existing validation records remains
Windows/MSVC evidence and is not reclassified as Linux evidence.

## Linux/POSIX ownership boundary

On Windows, `NativeSocket` remains `SOCKET` and the invalid value remains
`INVALID_SOCKET`. On POSIX, `NativeSocket` is an `int` file descriptor and the
invalid value is `-1`. `INVALID_NATIVE_SOCKET` gives shared ownership code and
callers a platform-neutral empty-state value.

`src/transport/socket_handle_posix.cpp` preserves the existing RAII and move-only
contract and releases an owned descriptor once with `close`. A failed `close` is
reported to standard error from the nonthrowing destructor. It is never retried,
including after `EINTR`, because Linux may already have released and reused the
descriptor.

`src/transport/winsock_runtime_posix.cpp` supplies a no-op `WinsockRuntime` for
source compatibility. POSIX sockets require no process-wide startup or cleanup;
the type remains operationally significant only on Windows.

## Validation boundary

The shared ownership cases cover empty ownership, acquisition and deterministic
release, move construction and assignment, empty and self moves, and exception
unwinding using real native stream sockets. Closed Windows sockets are checked
with the Winsock error contract; closed Linux descriptors are checked for
`EBADF`. Exact native error numbers are not treated as cross-platform contracts.

The Winsock runtime lifecycle case remains Windows-only. Linux connection
establishment and listener behavior were added and validated in a subsequent
increment; see `tcp-establishment-linux-validation.md`. Linux byte transport,
framing over TCP, and client/server application behavior remain outside the
current Linux implementation.
