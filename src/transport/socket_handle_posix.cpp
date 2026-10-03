#include "rds/transport/socket_handle.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <utility>

namespace rds::transport {

SocketHandle::SocketHandle(NativeSocket socket) noexcept : socket_(socket) {}

SocketHandle::~SocketHandle() noexcept { close(); }

SocketHandle::SocketHandle(SocketHandle&& other) noexcept
    : socket_(std::exchange(other.socket_, INVALID_NATIVE_SOCKET)) {}

SocketHandle& SocketHandle::operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::exchange(other.socket_, INVALID_NATIVE_SOCKET);
    }
    return *this;
}

NativeSocket SocketHandle::nativeHandle() const noexcept { return socket_; }

void SocketHandle::close() noexcept {
    const auto socket = std::exchange(socket_, INVALID_NATIVE_SOCKET);
    if (socket != INVALID_NATIVE_SOCKET && ::close(socket) == -1) {
        const int error = errno;
        // Do not retry close after EINTR: on Linux the descriptor may already be
        // released and reused by another thread.
        std::fprintf(stderr, "close failed (POSIX error %d: %s)\n",
                     error, std::strerror(error));
    }
}

} // namespace rds::transport
