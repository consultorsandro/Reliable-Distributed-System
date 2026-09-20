#include "rds/transport/socket_handle.hpp"

#include <cstdio>
#include <utility>

namespace rds::transport {

SocketHandle::SocketHandle(NativeSocket socket) noexcept : socket_(socket) {}

SocketHandle::~SocketHandle() noexcept { close(); }

SocketHandle::SocketHandle(SocketHandle&& other) noexcept
    : socket_(std::exchange(other.socket_, INVALID_SOCKET)) {}

SocketHandle& SocketHandle::operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::exchange(other.socket_, INVALID_SOCKET);
    }
    return *this;
}

NativeSocket SocketHandle::nativeHandle() const noexcept { return socket_; }

void SocketHandle::close() noexcept {
    const auto socket = std::exchange(socket_, INVALID_SOCKET);
    if (socket != INVALID_SOCKET && ::closesocket(socket) == SOCKET_ERROR) {
        const auto error = ::WSAGetLastError();
        std::fprintf(stderr, "closesocket failed (Winsock error %d)\n", error);
    }
}

} // namespace rds::transport
