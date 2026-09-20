#include "rds/transport/tcp_listener.hpp"

#include <ws2tcpip.h>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <limits>

namespace rds::transport {
namespace {

[[noreturn]] void fail(const char* operation) {
    const int error = ::WSAGetLastError();
    throw std::system_error(error, std::system_category(), operation);
}

sockaddr_in endpoint(std::string_view address, std::uint16_t port) {
    if (address.empty() || address.size() >= INET_ADDRSTRLEN ||
        address.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Expected a numeric IPv4 address");
    }
    std::array<char, INET_ADDRSTRLEN> text{};
    std::copy(address.begin(), address.end(), text.begin());
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = ::htons(port);
    const int parsed = ::InetPtonA(AF_INET, text.data(), &result.sin_addr);
    if (parsed == 0) throw std::invalid_argument("Expected a numeric IPv4 address");
    if (parsed == -1) fail("InetPtonA");
    return result;
}

SocketHandle makeSocket() {
    SocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.nativeHandle() == INVALID_SOCKET) fail("socket");
    return socket;
}

} // namespace

TcpListener::TcpListener(std::string_view address, std::uint16_t port) {
    const auto local = endpoint(address, port);
    auto socket = makeSocket();
    // Prevent another socket from taking over this address/port on Windows.
    const BOOL exclusive = TRUE;
    if (::setsockopt(socket.nativeHandle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
        fail("setsockopt(SO_EXCLUSIVEADDRUSE)");
    if (::bind(socket.nativeHandle(), reinterpret_cast<const sockaddr*>(&local),
               sizeof(local)) == SOCKET_ERROR) fail("bind");
    if (::listen(socket.nativeHandle(), SOMAXCONN) == SOCKET_ERROR) fail("listen");
    socket_ = std::move(socket);
}

std::uint16_t TcpListener::localPort() const {
    sockaddr_in local{};
    int size = sizeof(local);
    if (::getsockname(socket_.nativeHandle(), reinterpret_cast<sockaddr*>(&local),
                      &size) == SOCKET_ERROR) fail("getsockname");
    return ::ntohs(local.sin_port);
}

TcpConnection TcpListener::accept() {
    SocketHandle accepted(::accept(socket_.nativeHandle(), nullptr, nullptr));
    if (accepted.nativeHandle() == INVALID_SOCKET) fail("accept");
    return TcpConnection(std::move(accepted));
}

TcpConnection connectTcp(std::string_view address, std::uint16_t port) {
    const auto remote = endpoint(address, port);
    auto socket = makeSocket();
    if (::connect(socket.nativeHandle(), reinterpret_cast<const sockaddr*>(&remote),
                  sizeof(remote)) == SOCKET_ERROR) fail("connect");
    return TcpConnection(std::move(socket));
}

void TcpConnection::sendAll(std::span<const std::byte> data) {
    while (!data.empty()) {
        const auto length = (std::min)(data.size(),
            static_cast<std::size_t>((std::numeric_limits<int>::max)()));
        const int sent = ::send(socket_.nativeHandle(),
            reinterpret_cast<const char*>(data.data()), static_cast<int>(length), 0);
        if (sent == SOCKET_ERROR) fail("send");
        // Zero is not SOCKET_ERROR: there is no valid last-error code to report.
        // Refuse to spin forever if a nonempty send makes no forward progress.
        if (sent == 0) throw std::runtime_error("send made no progress for nonempty input");
        data = data.subspan(static_cast<std::size_t>(sent));
    }
}

std::size_t TcpConnection::receive(std::span<std::byte> buffer) {
    if (buffer.empty()) throw std::invalid_argument("receive requires a nonempty buffer");
    const auto length = (std::min)(buffer.size(),
        static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const int received = ::recv(socket_.nativeHandle(),
        reinterpret_cast<char*>(buffer.data()), static_cast<int>(length), 0);
    if (received == SOCKET_ERROR) fail("recv");
    return static_cast<std::size_t>(received);
}

} // namespace rds::transport
