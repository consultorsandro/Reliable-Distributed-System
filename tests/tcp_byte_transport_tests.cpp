#include "rds/transport/tcp_listener.hpp"
#include "rds/transport/winsock_runtime.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <vector>

using namespace rds::transport;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Pair {
    TcpListener listener{"127.0.0.1", 0};
    TcpConnection client = connectTcp("127.0.0.1", listener.localPort());
    TcpConnection server = listener.accept();
};
constexpr std::array payload{std::byte{0x00}, std::byte{0x01}, std::byte{0x7f},
                             std::byte{0x80}, std::byte{0xff}};

// Compare reconstructed stream bytes, never send/receive call boundaries.
std::size_t collect(TcpConnection& receiver, std::span<const std::byte> expected,
                    std::size_t capacity) {
    std::vector<std::byte> buffer(capacity + 2, std::byte{0x5a});
    std::size_t offset = 0;
    std::size_t calls = 0;
    while (offset < expected.size()) {
        std::fill(buffer.begin(), buffer.end(), std::byte{0x5a});
        auto target = std::span(buffer).subspan(1, capacity);
        const auto count = receiver.receive(target);
        require(count > 0, "Unexpected EOF before complete payload");
        require(count <= capacity && count <= expected.size() - offset, "Invalid received size");
        require(std::equal(target.begin(), target.begin() + count, expected.begin() + offset),
                "Binary stream mismatch");
        require(buffer.front() == std::byte{0x5a} && buffer.back() == std::byte{0x5a},
                "Receive wrote outside the supplied span");
        require(std::all_of(target.begin() + count, target.end(),
                           [](std::byte value) { return value == std::byte{0x5a}; }),
                "Receive changed bytes beyond returned count");
        offset += count;
        ++calls;
    }
    return calls;
}
template<class F> void invalidSocket(F operation, std::string_view name) {
    try { operation(); }
    catch (const std::system_error& error) {
        require(error.code().value() == WSAENOTSOCK, "Expected WSAENOTSOCK");
        require(error.code().category() == std::system_category(), "Expected native error category");
        require(std::string_view(error.what()).find(name) != std::string_view::npos,
                "Missing operation name");
        std::cout << "Expected " << name << " failure: " << error.what()
                  << " [" << error.code().value() << "]\n";
        return;
    }
    throw std::runtime_error("Expected native byte-operation failure");
}
void run(std::string_view name) {
    const WinsockRuntime runtime;
    Pair pair;
    if (name == "empty_send") {
        pair.client.sendAll({});
        pair.client.sendAll(payload);
        collect(pair.server, payload, 64);
    } else if (name == "empty_receive") {
        bool rejected = false;
        try { (void)pair.server.receive({}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Empty receive must reject instead of returning EOF");
        pair.client.sendAll(payload);
        collect(pair.server, payload, 64);
    } else if (name == "binary") {
        pair.client.sendAll(payload);
        // Peer remains open and capacity exceeds all pending data: do not fill it.
        collect(pair.server, payload, 64);
    } else if (name == "bidirectional") {
        pair.client.sendAll(payload);
        collect(pair.server, payload, 64);
        auto reverse = payload;
        std::reverse(reverse.begin(), reverse.end());
        pair.server.sendAll(reverse);
        collect(pair.client, reverse, 64);
    } else if (name == "repeated_reads") {
        std::vector<std::byte> large(4096);
        for (std::size_t i = 0; i < large.size(); ++i)
            large[i] = static_cast<std::byte>(i % 256);
        pair.client.sendAll(large);
        const auto calls = collect(pair.server, large, 31);
        require(calls >= (large.size() + 30) / 31, "Small buffer must force repeated reads");
        std::cout << "Reconstructed 4096 binary bytes using " << calls << " reads (capacity 31)\n";
    } else if (name == "sequential_sends") {
        const auto bytes = std::span(payload);
        pair.client.sendAll(bytes.first(1));
        pair.client.sendAll(bytes.subspan(1, 2));
        pair.client.sendAll(bytes.subspan(3));
        collect(pair.server, payload, 3);
    } else if (name == "orderly_closure") {
        {
            auto client = std::move(pair.client);
            client.sendAll(payload);
            collect(pair.server, payload, 3);
        } // All bytes consumed before orderly close; no unread inbound data.
        std::array<std::byte, 8> buffer;
        buffer.fill(std::byte{0x5a});
        require(pair.server.receive(buffer) == 0, "Orderly EOF must return zero");
        require(std::all_of(buffer.begin(), buffer.end(),
                           [](std::byte value) { return value == std::byte{0x5a}; }),
                "EOF must not alter destination bytes");
    } else if (name == "moved_from") {
        auto owner = std::move(pair.client);
        pair.client.sendAll({}); // No native operation even without a socket.
        invalidSocket([&] { pair.client.sendAll(payload); }, "send");
        std::array<std::byte, 8> buffer{};
        invalidSocket([&] { (void)pair.client.receive(buffer); }, "recv");
        owner.sendAll(payload);
        collect(pair.server, payload, 64);
    } else throw std::invalid_argument("Unknown byte transport test");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected byte transport test name");
        run(argv[1]);
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "TCP byte transport test failed: " << error.what() << '\n';
        return 1;
    }
}
