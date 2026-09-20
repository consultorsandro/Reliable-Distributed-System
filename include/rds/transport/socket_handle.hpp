#pragma once

#include <winsock2.h>

namespace rds::transport {

using NativeSocket = SOCKET;

// Exclusive ownership only. An initialized WinsockRuntime must outlive this owner.
class SocketHandle {
public:
    SocketHandle() noexcept = default;
    // Adopts sole ownership; socket must be INVALID_SOCKET or a live socket with
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
    NativeSocket socket_ = INVALID_SOCKET;
};

} // namespace rds::transport
