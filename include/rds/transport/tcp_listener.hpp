#pragma once

#include "rds/transport/tcp_connection.hpp"

namespace rds::transport {

// Exclusive listening socket owner. On Windows, WinsockRuntime must outlive it.
class TcpListener {
public:
    // Numeric IPv4; port zero requests an OS-assigned port. Bind exclusivity is
    // platform-specific: Windows uses SO_EXCLUSIVEADDRUSE; POSIX uses plain bind.
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
