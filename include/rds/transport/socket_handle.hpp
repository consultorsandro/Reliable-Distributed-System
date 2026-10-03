#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace rds::transport {

#ifdef _WIN32
using NativeSocket = SOCKET;
inline constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
#else
using NativeSocket = int;
inline constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
#endif

// Exclusive ownership only. On Windows, an initialized WinsockRuntime must
// outlive this owner; POSIX requires no socket runtime initialization.
class SocketHandle {
public:
    SocketHandle() noexcept = default;
    // Adopts sole ownership; socket must be INVALID_NATIVE_SOCKET or a live socket with
    // no other owner. Keep the default disabled SO_LINGER setting for cleanup.
    explicit SocketHandle(NativeSocket socket) noexcept;
    ~SocketHandle() noexcept;

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept;
    SocketHandle& operator=(SocketHandle&& other) noexcept;

    // Borrowed access only: callers must not close or independently adopt it.
    [[nodiscard]] NativeSocket nativeHandle() const noexcept;

private:
    void close() noexcept;
    NativeSocket socket_ = INVALID_NATIVE_SOCKET;
};

} // namespace rds::transport
