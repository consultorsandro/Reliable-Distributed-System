#pragma once

#include "rds/transport/socket_handle.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace rds::transport {

class TcpListener;

// Exclusively owns an established socket. WinsockRuntime must outlive this owner.
class TcpConnection {
public:
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) noexcept = default;
    TcpConnection& operator=(TcpConnection&&) noexcept = default;

    // Blocking. Success means local socket acceptance, not peer processing.
    // Empty input is a no-op, including on a moved-from owner. Failure may
    // follow partial acceptance; no progress count or automatic retry is provided.
    void sendAll(std::span<const std::byte> data);

    // One bounded native receive: positive count = data, zero = orderly EOF.
    // Empty buffers throw invalid_argument; native errors throw system_error.
    // Nonempty byte operations on moved-from owners report WSAENOTSOCK.
    [[nodiscard]] std::size_t receive(std::span<std::byte> buffer);

    // Borrowed only: do not close, adopt, or change socket ownership/settings.
    // A moved-from connection returns INVALID_SOCKET.
    [[nodiscard]] NativeSocket nativeHandle() const noexcept { return socket_.nativeHandle(); }

private:
    explicit TcpConnection(SocketHandle socket) noexcept : socket_(std::move(socket)) {}
    friend class TcpListener;
    friend TcpConnection connectTcp(std::string_view address, std::uint16_t port);
    SocketHandle socket_;
};

// Blocking, numeric IPv4 only. Caller must keep WinsockRuntime alive.
[[nodiscard]] TcpConnection connectTcp(std::string_view address, std::uint16_t port);

} // namespace rds::transport
