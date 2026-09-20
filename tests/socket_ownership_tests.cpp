#include "rds/transport/socket_handle.hpp"
#include "rds/transport/winsock_runtime.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

using namespace rds::transport;

static_assert(!std::is_copy_constructible_v<SocketHandle>);
static_assert(!std::is_copy_assignable_v<SocketHandle>);
static_assert(std::is_nothrow_move_constructible_v<SocketHandle>);
static_assert(std::is_nothrow_move_assignable_v<SocketHandle>);
static_assert(std::is_nothrow_destructible_v<SocketHandle>);
static_assert(!std::is_copy_constructible_v<WinsockRuntime>);
static_assert(!std::is_copy_assignable_v<WinsockRuntime>);
static_assert(!std::is_move_constructible_v<WinsockRuntime>);
static_assert(!std::is_move_assignable_v<WinsockRuntime>);
static_assert(std::is_nothrow_destructible_v<WinsockRuntime>);

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

SocketHandle make_socket() {
    const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == INVALID_SOCKET) {
        throw std::system_error(::WSAGetLastError(), std::system_category(), "socket");
    }
    return SocketHandle(socket);
}

void require_live(NativeSocket socket) {
    int type = 0;
    int size = sizeof(type);
    require(::getsockopt(socket, SOL_SOCKET, SO_TYPE,
                        reinterpret_cast<char*>(&type), &size) == 0,
            "Owned socket must remain usable");
    require(type == SOCK_STREAM, "Expected stream socket");
}

// No socket creation occurs between close and this probe, preventing handle reuse
// by this single-threaded test. This observes closure, not exact close-call counts.
void require_closed(NativeSocket socket) {
    int type = 0;
    int size = sizeof(type);
    const int result = ::getsockopt(socket, SOL_SOCKET, SO_TYPE,
                                   reinterpret_cast<char*>(&type), &size);
    const int error = ::WSAGetLastError();
    require(result == SOCKET_ERROR && error == WSAENOTSOCK,
            "Released handle must no longer identify a socket");
}

void run(std::string_view name) {
    if (name == "empty") {
        const SocketHandle empty;
        const SocketHandle invalid(INVALID_SOCKET);
        require(empty.nativeHandle() == INVALID_SOCKET, "Default state is empty");
        require(invalid.nativeHandle() == INVALID_SOCKET, "Invalid adoption is empty");
        return; // Also exercises destruction without Winsock initialization.
    }
    if (name == "runtime_lifecycle") {
        for (int iteration = 0; iteration < 2; ++iteration) {
            {
                const WinsockRuntime outer;
                {
                    const WinsockRuntime inner;
                    const auto socket = make_socket();
                    require_live(socket.nativeHandle());
                }
                const auto socket = make_socket();
                require_live(socket.nativeHandle());
            }
            const auto raw = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            const int error = ::WSAGetLastError();
            const SocketHandle unexpected(raw);
            require(raw == INVALID_SOCKET && error == WSANOTINITIALISED,
                    "Last runtime destruction must balance startup");
        }
        return;
    }
    const WinsockRuntime runtime;
    if (name == "acquisition") {
        NativeSocket raw;
        {
            const auto owner = make_socket();
            raw = owner.nativeHandle();
            require_live(raw);
        }
        require_closed(raw);
    } else if (name == "move_construction") {
        auto source = make_socket();
        const auto raw = source.nativeHandle();
        {
            const SocketHandle destination(std::move(source));
            require(source.nativeHandle() == INVALID_SOCKET, "Moved source is empty");
            require(destination.nativeHandle() == raw, "Move preserves handle");
            require_live(raw);
        }
        require_closed(raw);
    } else if (name == "move_assignment") {
        auto destination = make_socket();
        const auto replaced = destination.nativeHandle();
        NativeSocket transferred;
        {
            auto source = make_socket();
            transferred = source.nativeHandle();
            destination = std::move(source);
            require(source.nativeHandle() == INVALID_SOCKET, "Assigned source is empty");
            require(destination.nativeHandle() == transferred, "Assignment preserves handle");
            require_closed(replaced);
        }
        require_live(transferred); // Moved-from destruction must not close destination.
        destination = SocketHandle{};
        require_closed(transferred);
    } else if (name == "empty_move") {
        SocketHandle source;
        SocketHandle destination(std::move(source));
        require(destination.nativeHandle() == INVALID_SOCKET, "Empty move stays empty");
        auto owned = make_socket();
        const auto raw = owned.nativeHandle();
        owned = std::move(destination);
        require(owned.nativeHandle() == INVALID_SOCKET, "Empty assignment clears owner");
        require_closed(raw);
    } else if (name == "self_move") {
        auto owner = make_socket();
        const auto raw = owner.nativeHandle();
        auto& alias = owner;
        owner = std::move(alias);
        require(owner.nativeHandle() == raw, "Self move preserves ownership");
        require_live(raw);
    } else if (name == "unwinding") {
        struct Expected {};
        NativeSocket raw = INVALID_SOCKET;
        try {
            const auto owner = make_socket();
            raw = owner.nativeHandle();
            throw Expected{};
        } catch (const Expected&) {
        }
        require_closed(raw);
    } else {
        throw std::invalid_argument("Unknown ownership test case");
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected one ownership test case name");
        run(argv[1]);
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Socket ownership test failed: " << error.what() << '\n';
        return 1;
    }
}
