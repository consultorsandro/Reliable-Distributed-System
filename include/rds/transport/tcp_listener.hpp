#pragma once

#include "rds/transport/tcp_connection.hpp"

namespace rds::transport {

// Exclusive listening socket owner. WinsockRuntime must outlive this owner.
class TcpListener {
public:
    // Numeric IPv4; port zero requests an OS-assigned port.
    TcpListener(std::string_view address, std::uint16_t port);
    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&&) noexcept = default;
    TcpListener& operator=(TcpListener&&) noexcept = default;

    [[nodiscard]] std::uint16_t localPort() const;
    // Blocking; returned connection owns a distinct socket.
    [[nodiscard]] TcpConnection accept();

private:
    SocketHandle socket_;
};

} // namespace rds::transport
