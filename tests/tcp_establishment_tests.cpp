#include "rds/transport/tcp_listener.hpp"
#include "rds/transport/winsock_runtime.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

using namespace rds::transport;
static_assert(!std::is_copy_constructible_v<TcpConnection>);
static_assert(!std::is_copy_assignable_v<TcpConnection>);
static_assert(std::is_nothrow_move_constructible_v<TcpConnection>);
static_assert(std::is_nothrow_move_assignable_v<TcpConnection>);
static_assert(std::is_nothrow_destructible_v<TcpConnection>);
static_assert(!std::is_constructible_v<TcpConnection, SocketHandle>);
static_assert(!std::is_copy_constructible_v<TcpListener>);
static_assert(!std::is_copy_assignable_v<TcpListener>);
static_assert(std::is_nothrow_move_constructible_v<TcpListener>);
static_assert(std::is_nothrow_move_assignable_v<TcpListener>);
static_assert(std::is_nothrow_destructible_v<TcpListener>);

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void connected(const TcpConnection& connection) {
    sockaddr_in peer{};
    int size = sizeof(peer);
    require(::getpeername(connection.nativeHandle(), reinterpret_cast<sockaddr*>(&peer), &size) == 0,
            "Expected an established peer");
    require(peer.sin_family == AF_INET && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
            "Expected loopback peer");
}
void closed(NativeSocket socket) {
    int type = 0;
    int size = sizeof(type);
    const int result = ::getsockopt(socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&type), &size);
    const int error = ::WSAGetLastError();
    require(result == SOCKET_ERROR && error == WSAENOTSOCK, "Expected closed socket");
}
template<class F> void systemFailure(F operation, std::string_view name, int code) {
    try { operation(); }
    catch (const std::system_error& error) {
        std::cout << "Expected failure: " << error.what() << " [" << error.code().value() << "]\n";
        require(error.code().value() == code, "Unexpected native error code");
        require(std::string_view(error.what()).find(name) != std::string_view::npos,
                "Missing operation diagnostic");
        return;
    }
    throw std::runtime_error("Expected system_error");
}
void run(std::string_view name) {
    if (name == "no_runtime") {
        systemFailure([] { (void)connectTcp("127.0.0.1", 1); }, "socket", WSANOTINITIALISED);
        return;
    }
    const WinsockRuntime runtime;
    if (name == "invalid_address") {
        for (const auto address : {std::string_view{}, std::string_view("999.1.2.3"),
             std::string_view("localhost"), std::string_view("::1"),
             std::string_view("127.0.0.1\0x", 11), std::string_view("1234567890123456")}) {
            for (bool listener : {false, true}) {
                bool rejected = false;
                try {
                    if (listener) { const TcpListener value(address, 0); }
                    else { (void)connectTcp(address, 1); }
                } catch (const std::invalid_argument&) { rejected = true; }
                require(rejected, "Invalid address must be rejected locally");
            }
        }
        return;
    }
    if (name == "refusal") {
        // Retain an exclusively bound, non-listening socket: no close/rebind race.
        SocketHandle reserved(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        require(reserved.nativeHandle() != INVALID_SOCKET, "Reservation socket failed");
        const BOOL exclusive = TRUE;
        require(::setsockopt(reserved.nativeHandle(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                            reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == 0,
                "Reservation exclusivity failed");
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(::bind(reserved.nativeHandle(), reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0,
                "Reservation bind failed");
        int size = sizeof(local);
        require(::getsockname(reserved.nativeHandle(), reinterpret_cast<sockaddr*>(&local), &size) == 0,
                "Reservation port discovery failed");
        systemFailure([&] { (void)connectTcp("127.0.0.1", ntohs(local.sin_port)); },
                      "connect", WSAECONNREFUSED);
        return;
    }
    TcpListener listener("127.0.0.1", 0);
    const auto port = listener.localPort();
    require(port != 0, "Expected OS-assigned port");
    if (name == "listener") return;
    if (name == "bind_failure") {
        systemFailure([&] { const TcpListener duplicate("127.0.0.1", port); }, "bind", WSAEADDRINUSE);
        return;
    }
    if (name == "listener_moves") {
        TcpListener moved(std::move(listener));
        systemFailure([&] { (void)listener.accept(); }, "accept", WSAENOTSOCK);
        systemFailure([&] { (void)listener.localPort(); }, "getsockname", WSAENOTSOCK);
        TcpListener destination("127.0.0.1", 0);
        destination = std::move(moved);
        require(destination.localPort() == port, "Listener move must preserve bound port");
        auto client = connectTcp("127.0.0.1", port);
        auto server = destination.accept();
        connected(client); connected(server);
        return;
    }
    if (name == "listener_destruction") {
        std::optional<TcpConnection> accepted;
        std::optional<TcpConnection> client;
        {
            TcpListener temporary("127.0.0.1", 0);
            client.emplace(connectTcp("127.0.0.1", temporary.localPort()));
            accepted.emplace(temporary.accept());
        }
        connected(*client); connected(*accepted);
        return;
    }
    auto client = connectTcp("127.0.0.1", port);
    auto accepted = listener.accept();
    connected(client); connected(accepted);
    if (name == "establishment") return;
    if (name == "independence") {
        NativeSocket raw;
        {
            auto owner = std::move(accepted);
            raw = owner.nativeHandle();
        }
        closed(raw);
        auto secondClient = connectTcp("127.0.0.1", port);
        auto secondAccepted = listener.accept();
        connected(secondClient); connected(secondAccepted);
    } else if (name == "connection_moves") {
        const auto raw = accepted.nativeHandle();
        auto moved = std::move(accepted);
        require(accepted.nativeHandle() == INVALID_SOCKET, "Moved source must be empty");
        require(moved.nativeHandle() == raw, "Move must transfer socket");
        const auto replaced = client.nativeHandle();
        client = std::move(moved);
        closed(replaced); // No intervening socket allocation.
        require(moved.nativeHandle() == INVALID_SOCKET, "Assigned source must be empty");
        require(client.nativeHandle() == raw, "Assignment must transfer socket");
        connected(client);
    } else throw std::invalid_argument("Unknown test case");
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected test case name");
        run(argv[1]);
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TCP establishment test failed: " << error.what() << '\n';
        return 1;
    }
}
