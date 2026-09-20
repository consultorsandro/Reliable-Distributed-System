#pragma once

#include "rds/protocol/message.hpp"

#include <span>

namespace rds::protocol {

class FrameEncoder {
public:
    // Returns an owned frame. Invalid messages throw std::invalid_argument.
    static std::vector<std::byte> encode(const Message& message);
};

class FrameDecoder {
public:
    // Requires exactly one complete frame and copies its payload into the result.
    // Invalid frames throw std::invalid_argument.
    static Message decode(std::span<const std::byte> frame);
};

} // namespace rds::protocol
