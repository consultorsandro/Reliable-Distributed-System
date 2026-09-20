#include "rds/protocol/framing.hpp"

#include <stdexcept>

namespace rds::protocol {
namespace {

constexpr std::size_t LENGTH_FIELD_SIZE = 4;

bool is_known_type(MessageType type) {
    return type == MessageType::Ping || type == MessageType::Pong;
}

} // namespace

std::vector<std::byte> FrameEncoder::encode(const Message& message) {
    if (!is_known_type(message.type)) {
        throw std::invalid_argument("Unknown message type");
    }
    // Check before addition so even an extreme payload size cannot overflow.
    if (message.payload.size() > MAX_MESSAGE_SIZE - 1) {
        throw std::invalid_argument("Message body exceeds maximum size");
    }

    const auto length = static_cast<std::uint32_t>(1 + message.payload.size());
    std::vector<std::byte> frame;
    frame.reserve(LENGTH_FIELD_SIZE + length);
    for (int shift = 24; shift >= 0; shift -= 8) {
        frame.push_back(static_cast<std::byte>((length >> shift) & 0xffu));
    }
    frame.push_back(static_cast<std::byte>(message.type));
    frame.insert(frame.end(), message.payload.begin(), message.payload.end());
    return frame;
}

Message FrameDecoder::decode(std::span<const std::byte> frame) {
    if (frame.size() < LENGTH_FIELD_SIZE + 1) {
        throw std::invalid_argument("Frame is shorter than the minimum size");
    }

    std::uint32_t length = 0;
    for (std::size_t index = 0; index < LENGTH_FIELD_SIZE; ++index) {
        length = (length << 8) | std::to_integer<std::uint32_t>(frame[index]);
    }
    if (length == 0) {
        throw std::invalid_argument("Message body must contain a message type");
    }
    if (length > MAX_MESSAGE_SIZE) {
        throw std::invalid_argument("Declared message body exceeds maximum size");
    }
    if (frame.size() - LENGTH_FIELD_SIZE != length) {
        throw std::invalid_argument("Declared length does not match frame size");
    }

    const auto type = static_cast<MessageType>(
        std::to_integer<std::uint8_t>(frame[LENGTH_FIELD_SIZE]));
    if (!is_known_type(type)) {
        throw std::invalid_argument("Unknown message type");
    }

    const auto payload = frame.subspan(LENGTH_FIELD_SIZE + 1);
    return Message{type, std::vector<std::byte>(payload.begin(), payload.end())};
}

} // namespace rds::protocol
