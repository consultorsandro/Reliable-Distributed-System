#pragma once

#include "rds/protocol/message.hpp"

#include <span>

namespace rds::protocol {

class FrameStreamDecoder {
public:
    // Borrows input during this call; incomplete bytes and returned payloads are owned.
    // Malformed input throws std::invalid_argument and clears buffered state.
    // On failure, no messages from this call are returned and remaining input is discarded.
    std::vector<Message> consume(std::span<const std::byte> bytes);
    // Validate end of input: throws invalid_argument if a partial frame remains.
    // Does not mutate state or seal the decoder; caller must invoke at stream EOF.
    void finish() const;
    void reset() noexcept;

private:
    std::vector<std::byte> buffer_;
};

} // namespace rds::protocol
