#include "rds/protocol/frame_stream_decoder.hpp"
#include "rds/protocol/framing.hpp"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace rds::protocol;

namespace {

using Bytes = std::vector<std::byte>;

Bytes bytes(std::initializer_list<unsigned int> values) {
    Bytes result;
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

void require(bool condition, const char* name) {
    if (!condition) {
        throw std::runtime_error(name);
    }
}

template <typename Action>
void require_rejection(Action action, const char* name) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(std::string(name) + ": expected invalid_argument");
}

void require_message(const std::vector<Message>& messages, const Message& expected) {
    require(messages.size() == 1, "Exactly one message returned");
    require(messages[0].type == expected.type && messages[0].payload == expected.payload,
            "Message type and binary payload preserved");
}

void test_complete_and_empty_input() {
    FrameStreamDecoder decoder;
    require(decoder.consume({}).empty(), "Empty input returns no messages");
    require_message(decoder.consume(bytes({0, 0, 0, 1, 1})), {MessageType::Ping, {}});
    require_message(decoder.consume(bytes({0, 0, 0, 1, 2})), {MessageType::Pong, {}});
    require(decoder.consume({}).empty(), "Completed messages are not emitted again");
}

void test_every_split() {
    const Message message{MessageType::Pong, bytes({0, 0x80, 0xff, 1, 2})};
    const auto frame = FrameEncoder::encode(message);
    for (std::size_t split = 1; split < frame.size(); ++split) {
        FrameStreamDecoder decoder;
        auto prefix = Bytes(frame.begin(), frame.begin() + split);
        require(decoder.consume(prefix).empty(), "Partial header or body waits for input");
        std::fill(prefix.begin(), prefix.end(), std::byte{0xff});
        require(decoder.consume({}).empty(), "Empty input preserves partial frame");
        require_message(decoder.consume(std::span<const std::byte>(frame).subspan(split)), message);
    }
}

void test_one_byte_at_a_time() {
    const Message message{MessageType::Ping, bytes({0, 0xff, 0x80})};
    const auto frame = FrameEncoder::encode(message);
    FrameStreamDecoder decoder;
    for (std::size_t index = 0; index < frame.size(); ++index) {
        const auto messages = decoder.consume(std::span<const std::byte>(frame).subspan(index, 1));
        if (index + 1 == frame.size()) {
            require_message(messages, message);
        } else {
            require(messages.empty(), "One-byte input waits until frame is complete");
        }
    }
}

void test_multiple_frames() {
    for (const std::size_t count : {2, 7}) {
        Bytes chunk;
        std::vector<Message> expected;
        for (std::size_t index = 0; index < count; ++index) {
            expected.push_back({index % 2 == 0 ? MessageType::Ping : MessageType::Pong,
                                Bytes(index, static_cast<std::byte>(index))});
            const auto frame = FrameEncoder::encode(expected.back());
            chunk.insert(chunk.end(), frame.begin(), frame.end());
        }
        FrameStreamDecoder decoder;
        const auto messages = decoder.consume(chunk);
        require(messages.size() == count, "All complete frames returned in one call");
        for (std::size_t index = 0; index < count; ++index) {
            require(messages[index].type == expected[index].type &&
                        messages[index].payload == expected[index].payload,
                    "Multiple messages preserve order and payloads");
        }
    }
}

void test_complete_then_partial() {
    const auto first = FrameEncoder::encode({MessageType::Ping, {}});
    const Message next{MessageType::Pong, bytes({0, 0xff, 0x80})};
    const auto second = FrameEncoder::encode(next);
    for (std::size_t split = 1; split < second.size(); ++split) {
        auto chunk = first;
        chunk.insert(chunk.end(), second.begin(), second.begin() + split);
        FrameStreamDecoder decoder;
        require_message(decoder.consume(chunk), {MessageType::Ping, {}});
        require_message(decoder.consume(std::span<const std::byte>(second).subspan(split)), next);
    }
}

void test_invalid_headers() {
    for (const auto& header : {bytes({0, 0, 0, 0}), bytes({0, 1, 0, 1}),
                               bytes({0xff, 0xff, 0xff, 0xff})}) {
        for (std::size_t split = 1; split < 4; ++split) {
            FrameStreamDecoder decoder;
            require(decoder.consume(std::span<const std::byte>(header).first(split)).empty(),
                    "Incomplete invalid header waits for fourth byte");
            require_rejection([&] {
                decoder.consume(std::span<const std::byte>(header).subspan(split));
            }, "Invalid length rejected immediately at complete header");
            require_message(decoder.consume(bytes({0, 0, 0, 1, 1})), {MessageType::Ping, {}});
        }
    }
}

void test_malformed_call_is_atomic() {
    for (const auto& malformed : {bytes({0, 0, 0, 1, 0xff}),
                                  bytes({0, 0, 0, 0}), bytes({0, 1, 0, 1})}) {
        FrameStreamDecoder decoder;
        const auto ping = bytes({0, 0, 0, 1, 1});
        const auto pong = bytes({0, 0, 0, 1, 2});
        require(decoder.consume(std::span<const std::byte>(ping).first(2)).empty(),
                "Valid partial frame buffered before malformed call");
        Bytes chunk(ping.begin() + 2, ping.end());
        chunk.insert(chunk.end(), malformed.begin(), malformed.end());
        chunk.insert(chunk.end(), pong.begin(), pong.end());
        require_rejection([&] { decoder.consume(chunk); },
                          "Valid message followed by malformed input throws atomically");
        require(decoder.consume({}).empty(), "No pending messages after malformed input");
        require_message(decoder.consume(pong), {MessageType::Pong, {}});
    }
    FrameStreamDecoder decoder;
    require(decoder.consume(bytes({0, 0, 0, 2, 0xff})).empty(), "Partial unknown frame buffered");
    require_rejection([&] { decoder.consume(bytes({0})); }, "Completed unknown type rejected");
    require_message(decoder.consume(bytes({0, 0, 0, 1, 1})), {MessageType::Ping, {}});
}

void test_finish_contract() {
    FrameStreamDecoder empty_decoder;
    empty_decoder.finish();

    const Message expected{MessageType::Pong, bytes({0x41, 0x42})};
    const auto frame = FrameEncoder::encode(expected);

    FrameStreamDecoder complete_decoder;
    require_message(complete_decoder.consume(frame), expected);
    complete_decoder.finish();

    for (const auto split : {std::size_t{2}, std::size_t{5}}) {
        FrameStreamDecoder decoder;

        const auto partial =
            std::span<const std::byte>(frame).first(split);
        require(decoder.consume(partial).empty(),
                "Partial frame emits no message");

        require_rejection([&] { decoder.finish(); },
                          "Incomplete frame rejected at end of input");

        const auto remainder =
            std::span<const std::byte>(frame).subspan(split);
        require_message(decoder.consume(remainder), expected);

        decoder.finish();
    }
}



void test_reset() {
    const auto frame = bytes({0, 0, 0, 3, 1, 0xff, 0x80});
    FrameStreamDecoder decoder;
    for (const std::size_t split : {2, 5}) {
        require(decoder.consume(std::span<const std::byte>(frame).first(split)).empty(),
                "Partial input buffered before reset");
        decoder.reset();
        decoder.reset();
        require_message(decoder.consume(frame), {MessageType::Ping, bytes({0xff, 0x80})});
    }
}

void test_maximum_message() {
    Message message{MessageType::Pong, Bytes(MAX_MESSAGE_SIZE - 1)};
    for (std::size_t index = 0; index < message.payload.size(); ++index) {
        message.payload[index] = static_cast<std::byte>(index % 256);
    }
    auto frame = FrameEncoder::encode(message);
    FrameStreamDecoder decoder;
    std::vector<Message> result;
    for (std::size_t offset = 0; offset < frame.size();) {
        const auto count = std::min(std::size_t{137}, frame.size() - offset);
        result = decoder.consume(std::span<const std::byte>(frame).subspan(offset, count));
        offset += count;
        if (offset < frame.size()) {
            require(result.empty(), "Maximum-sized partial message remains buffered");
        }
    }
    std::fill(frame.begin(), frame.end(), std::byte{0});
    decoder.reset();
    require_message(result, message);
}

} // namespace

int main() {
    try {
        test_complete_and_empty_input();
        test_every_split();
        test_one_byte_at_a_time();
        test_multiple_frames();
        test_complete_then_partial();
        test_invalid_headers();
        test_malformed_call_is_atomic();
        test_reset();
        test_maximum_message();
        test_finish_contract();
        std::cout << "All frame stream decoder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Frame stream decoder test failed: " << error.what() << '\n';
        return 1;
    }
}
