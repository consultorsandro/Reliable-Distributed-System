#include "rds/protocol/frame_stream_decoder.hpp"
#include "rds/protocol/framing.hpp"
#include "rds/transport/tcp_listener.hpp"
#include "rds/transport/winsock_runtime.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace rds::protocol;
using namespace rds::transport;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejected(F action) {
    try { action(); }
    catch (const std::invalid_argument& error) {
        std::cout << "Expected framing rejection: " << error.what() << '\n';
        return;
    }
    throw std::runtime_error("Expected framing invalid_argument");
}
struct Pair {
    TcpListener listener{"127.0.0.1", 0};
    TcpConnection client = connectTcp("127.0.0.1", listener.localPort());
    TcpConnection server = listener.accept();
};
Message sample(MessageType type = MessageType::Ping) {
    return {type, {std::byte{0}, std::byte{1}, std::byte{0x7f}, std::byte{0x80}, std::byte{0xff}}};
}
void equal(const std::vector<Message>& actual, const std::vector<Message>& expected) {
    require(actual.size() == expected.size(), "Message count mismatch");
    for (std::size_t i = 0; i < actual.size(); ++i)
        require(actual[i].type == expected[i].type && actual[i].payload == expected[i].payload,
                "Message type, order or binary payload mismatch");
}
// Bounded byte budget controls test phases, not native receive boundaries.
// Every positive receive is passed directly and exactly to the same decoder.
std::size_t pump(TcpConnection& peer, FrameStreamDecoder& decoder, std::size_t bytes,
                 std::size_t capacity, std::vector<Message>& output) {
    std::vector<std::byte> buffer(capacity);
    std::size_t calls = 0;
    while (bytes != 0) {
        auto destination = std::span(buffer).first((std::min)(bytes, capacity));
        const auto count = peer.receive(destination);
        require(count > 0 && count <= destination.size(), "Unexpected EOF/count");
        auto messages = decoder.consume(destination.first(count));
        for (auto& message : messages) output.push_back(std::move(message));
        bytes -= count;
        ++calls;
    }
    return calls;
}
void eof(TcpConnection& peer, FrameStreamDecoder& decoder, bool truncated) {
    std::byte buffer{};
    require(peer.receive({&buffer, 1}) == 0, "Expected orderly transport EOF");
    if (truncated) rejected([&] { decoder.finish(); });
    else decoder.finish();
}
void finish_contract() {
    FrameStreamDecoder decoder;
    decoder.finish();
    decoder.finish();
    const auto message = sample();
    const auto frame = FrameEncoder::encode(message);
    for (std::size_t split = 1; split < frame.size(); ++split) {
        require(decoder.consume(std::span(frame).first(split)).empty(), "Prefix emitted a message");
        rejected([&] { decoder.finish(); });
        // finish is a const validation, not an implicit reset or terminal state.
        equal(decoder.consume(std::span(frame).subspan(split)), {message});
        decoder.finish();
    }
    (void)decoder.consume(std::span(frame).first(2));
    decoder.reset();
    decoder.finish();
}
void run(std::string_view name) {
    if (name == "finish_contract") { finish_contract(); return; }
    const WinsockRuntime runtime;
    Pair pair;
    FrameStreamDecoder decoder;
    std::vector<Message> output;
    const auto message = sample();
    const auto frame = FrameEncoder::encode(message);
    if (name == "binary" || name == "one_byte") {
        pair.client.sendAll(frame);
        const auto capacity = name == "one_byte" ? 1u : 3u;
        const auto calls = pump(pair.server, decoder, frame.size(), capacity, output);
        require(calls >= (frame.size() + capacity - 1) / capacity, "Expected multiple reads");
        equal(output, {message});
        decoder.finish();
        std::cout << frame.size() << " frame bytes, " << calls << " reads\n";
    } else if (name == "multiple") {
        const std::vector<Message> expected{message, {MessageType::Pong, {}}, sample(MessageType::Pong)};
        std::size_t total = 0;
        for (const auto& item : expected) {
            const auto encoded = FrameEncoder::encode(item);
            pair.client.sendAll(encoded);
            total += encoded.size();
        }
        pump(pair.server, decoder, total, 64, output);
        equal(output, expected);
        decoder.finish();
    } else if (name == "complete_partial") {
        const auto next = sample(MessageType::Pong);
        const auto second = FrameEncoder::encode(next);
        auto prefix = frame;
        prefix.insert(prefix.end(), second.begin(), second.begin() + 6);
        pair.client.sendAll(prefix);
        pump(pair.server, decoder, prefix.size(), prefix.size(), output);
        equal(output, {message});
        rejected([&] { decoder.finish(); });
        pair.client.sendAll(std::span(second).subspan(6));
        pump(pair.server, decoder, second.size() - 6, 3, output);
        equal(output, {message, next});
        decoder.finish();
    } else if (name == "maximum") {
        Message maximum{MessageType::Pong, std::vector<std::byte>(MAX_MESSAGE_SIZE - 1)};
        for (std::size_t i = 0; i < maximum.payload.size(); ++i)
            maximum.payload[i] = static_cast<std::byte>(i % 256);
        const auto encoded = FrameEncoder::encode(maximum);
        require(encoded.size() == 65540, "Protocol maximum changed");
        std::size_t calls = 0;
        // Bounded producer/consumer phases avoid depending on socket send capacity.
        for (std::size_t offset = 0; offset < encoded.size();) {
            const auto count = (std::min)(std::size_t{1024}, encoded.size() - offset);
            pair.client.sendAll(std::span(encoded).subspan(offset, count));
            calls += pump(pair.server, decoder, count, 137, output);
            offset += count;
            if (offset < encoded.size()) require(output.empty(), "Premature maximum frame emission");
        }
        equal(output, {maximum});
        decoder.finish();
        std::cout << "65540 frame bytes reconstructed with " << calls << " bounded reads\n";
    } else if (name == "bidirectional") {
        pair.client.sendAll(frame);
        pump(pair.server, decoder, frame.size(), 3, output);
        equal(output, {message});
        decoder.finish();
        FrameStreamDecoder reverse;
        output.clear();
        const Message other{MessageType::Pong, {std::byte{0xff}, std::byte{0}}};
        const auto encoded = FrameEncoder::encode(other);
        pair.server.sendAll(encoded);
        pump(pair.client, reverse, encoded.size(), 2, output);
        equal(output, {other});
        reverse.finish();
    } else if (name == "complete_eof") {
        {
            auto sender = std::move(pair.client);
            sender.sendAll(frame);
            pump(pair.server, decoder, frame.size(), 3, output);
        }
        eof(pair.server, decoder, false);
        equal(output, {message});
    } else if (name == "truncated_eof") {
        for (std::size_t split = 1; split < frame.size(); ++split) {
            Pair truncated;
            FrameStreamDecoder partial;
            std::vector<Message> none;
            {
                auto sender = std::move(truncated.client);
                sender.sendAll(std::span(frame).first(split));
                pump(truncated.server, partial, split, 2, none);
            }
            require(none.empty(), "Truncated frame must never emit a message");
            eof(truncated.server, partial, true);
        }
    } else if (name == "malformed") {
        for (const auto& invalid : {std::vector<std::byte>{std::byte{0},std::byte{0},std::byte{0},std::byte{0}},
             std::vector<std::byte>{std::byte{0},std::byte{1},std::byte{0},std::byte{1}},
             std::vector<std::byte>{std::byte{0},std::byte{0},std::byte{0},std::byte{1},std::byte{0xff}}}) {
            pair.client.sendAll(invalid);
            rejected([&] { pump(pair.server, decoder, invalid.size(), 1, output); });
            require(output.empty(), "Malformed frame produced message");
            decoder.finish(); // Existing malformed-input behavior clears buffered state.
        }
        // Deliberately stop here: no attempt to resynchronize this TCP stream.
    } else throw std::invalid_argument("Unknown framing TCP test");
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected integration test name");
        run(argv[1]);
        std::cout << argv[1] << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Framing TCP test failed: " << error.what() << '\n';
        return 1;
    }
}
