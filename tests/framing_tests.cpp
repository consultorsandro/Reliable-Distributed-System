#include "rds/protocol/framing.hpp"

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

void test_default_message_rejection() {
    Message message;
    require(message.type == static_cast<MessageType>(0),
            "Default message type is zero");
    require_rejection([&] { FrameEncoder::encode(message); },
                      "Default-constructed message rejected");
}

void test_empty_messages() {
    const auto ping = bytes({0, 0, 0, 1, 1});
    const auto pong = bytes({0, 0, 0, 1, 2});
    require(FrameEncoder::encode({MessageType::Ping, {}}) == ping,
            "Encoding an empty Ping");
    require(FrameEncoder::encode({MessageType::Pong, {}}) == pong,
            "Encoding an empty Pong");
    const auto decoded_ping = FrameDecoder::decode(ping);
    const auto decoded_pong = FrameDecoder::decode(pong);
    require(decoded_ping.type == MessageType::Ping && decoded_ping.payload.empty(),
            "Decoding a complete Ping");
    require(decoded_pong.type == MessageType::Pong && decoded_pong.payload.empty(),
            "Decoding a complete Pong");
}

void test_payload() {
    Bytes payload;
    for (unsigned int value = 0; value < 256; ++value) {
        payload.push_back(static_cast<std::byte>(value));
    }
    for (const auto type : {MessageType::Ping, MessageType::Pong}) {
        auto expected = bytes({0, 0, 1, 1, static_cast<unsigned int>(type)});
        expected.insert(expected.end(), payload.begin(), payload.end());
        require(FrameEncoder::encode({type, payload}) == expected,
                "Binary payload encoding and big-endian length");
        auto decoded = FrameDecoder::decode(expected);
        require(decoded.type == type && decoded.payload == payload,
                "Binary payload preservation");
        expected.back() = std::byte{0};
        require(decoded.payload == payload, "Decoded payload owns its storage");
    }
}

void test_size_limits() {
    const Message maximum{MessageType::Ping, Bytes(MAX_MESSAGE_SIZE - 1, std::byte{0xff})};
    const auto encoded = FrameEncoder::encode(maximum);
    require(encoded.size() == MAX_MESSAGE_SIZE + 4, "Maximum frame size");
    require(Bytes(encoded.begin(), encoded.begin() + 4) == bytes({0, 1, 0, 0}),
            "Maximum body big-endian length");
    require(FrameDecoder::decode(encoded).payload == maximum.payload,
            "Maximum body accepted");
    require_rejection([] {
        FrameEncoder::encode({MessageType::Ping, Bytes(MAX_MESSAGE_SIZE)});
    }, "Oversized message rejected");
    auto oversized = bytes({0, 1, 0, 1, 1});
    oversized.resize(MAX_MESSAGE_SIZE + 5);
    require_rejection([&] { FrameDecoder::decode(oversized); },
                      "Oversized complete frame rejected");
    require_rejection([] { FrameDecoder::decode(bytes({0xff, 0xff, 0xff, 0xff, 1})); },
                      "Maximum unsigned declared length rejected");
}

void test_invalid_frames() {
    for (unsigned int value = 0; value < 256; ++value) {
        if (value == 1 || value == 2) {
            continue;
        }
        require_rejection([&] { FrameDecoder::decode(bytes({0, 0, 0, 1, value})); },
                          "Unknown decoded message type rejected");
        require_rejection([&] {
            FrameEncoder::encode({static_cast<MessageType>(value), {}});
        }, "Unknown encoded message type rejected");
    }
    for (const auto& frame : {bytes({0, 0, 0, 0, 1}),
                              bytes({0, 0, 0, 1, 1, 0}),
                              bytes({0, 0, 0, 3, 1, 0}),
                              bytes({0, 0, 0, 1, 1, 0, 0, 0, 1, 2})}) {
        require_rejection([&] { FrameDecoder::decode(frame); },
                          "Malformed or inconsistent length rejected");
    }
    const auto complete = bytes({0, 0, 0, 3, 1, 0x80, 0xff});
    for (std::size_t size = 0; size < complete.size(); ++size) {
        require_rejection([&] {
            FrameDecoder::decode(std::span<const std::byte>(complete).first(size));
        }, "Truncated frame rejected");
    }
}

} // namespace

int main() {
    try {
        test_default_message_rejection();
        test_empty_messages();
        test_payload();
        test_size_limits();
        test_invalid_frames();
        std::cout << "All framing tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Framing test failed: " << error.what() << '\n';
        return 1;
    }
}
