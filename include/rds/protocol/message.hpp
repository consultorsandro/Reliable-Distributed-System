#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rds::protocol {

enum class MessageType : std::uint8_t {
    Ping = 0x01,
    Pong = 0x02,
};

inline constexpr std::size_t MAX_MESSAGE_SIZE = 65'536;

struct Message {
    MessageType type{};
    std::vector<std::byte> payload;
};

} // namespace rds::protocol
