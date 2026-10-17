#include "rds/transport/tcp_listener.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace rds::transport {
namespace {

[[noreturn]] void fail(const char* operation) {
    // Call only directly on a native failure so no intervening call can replace errno.
    const int error = errno;
    throw std::system_error(error, std::system_category(), operation);
}

sockaddr_in endpoint(std::string_view address, std::uint16_t port) {
    // inet_pton requires a terminated string; reject embedded NULs before copying.
    if (address.empty() || address.size() >= INET_ADDRSTRLEN ||
        address.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Expected a numeric IPv4 address");
    }
    char text[INET_ADDRSTRLEN]{};
    address.copy(text, address.size());
    sockaddr_in result{};
    result.sin_family = AF_INET;
    result.sin_port = ::htons(port);
    const int parsed = ::inet_pton(AF_INET, text, &result.sin_addr);
    if (parsed == 0) throw std::invalid_argument("Expected a numeric IPv4 address");
    if (parsed == -1) fail("inet_pton");
    return result;
}

SocketHandle makeSocket() {
    SocketHandle socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket.nativeHandle() == INVALID_NATIVE_SOCKET) fail("socket");
    return socket;
}

} // namespace

TcpListener::TcpListener(std::string_view address, std::uint16_t port) {
    const auto local = endpoint(address, port);
    auto socket = makeSocket();
    // Linux deliberately uses plain bind semantics. SO_REUSEADDR and SO_REUSEPORT
    // would weaken the active-listener duplicate-bind policy required here.
    if (::bind(socket.nativeHandle(), reinterpret_cast<const sockaddr*>(&local),
               sizeof(local)) == -1) fail("bind");
    if (::listen(socket.nativeHandle(), SOMAXCONN) == -1) fail("listen");
    socket_ = std::move(socket);
}

std::uint16_t TcpListener::localPort() const {
    sockaddr_in local{};
    socklen_t size = sizeof(local);
    if (::getsockname(socket_.nativeHandle(), reinterpret_cast<sockaddr*>(&local),
                      &size) == -1) fail("getsockname");
    return ::ntohs(local.sin_port);
}

TcpConnection TcpListener::accept() {
    SocketHandle accepted(::accept(socket_.nativeHandle(), nullptr, nullptr));
    // EINTR is surfaced as system_error. Retrying accept would add an implicit
    // interruption policy not present in the public blocking contract.
    if (accepted.nativeHandle() == INVALID_NATIVE_SOCKET) fail("accept");
    return TcpConnection(std::move(accepted));
}

TcpConnection connectTcp(std::string_view address, std::uint16_t port) {
    const auto remote = endpoint(address, port);
    auto socket = makeSocket();
    // In particular, surface EINTR rather than turning it into an automatic retry.
    if (::connect(socket.nativeHandle(), reinterpret_cast<const sockaddr*>(&remote),
                  sizeof(remote)) == -1) fail("connect");
    return TcpConnection(std::move(socket));
}

} // namespace rds::transport
