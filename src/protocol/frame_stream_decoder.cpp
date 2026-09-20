#include "rds/protocol/frame_stream_decoder.hpp"

#include "rds/protocol/framing.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rds::protocol {
namespace {

constexpr std::size_t LENGTH_FIELD_SIZE = 4;
static_assert(MAX_MESSAGE_SIZE <= std::numeric_limits<std::size_t>::max() - LENGTH_FIELD_SIZE);

} // namespace

std::vector<Message> FrameStreamDecoder::consume(std::span<const std::byte> bytes) {
    std::vector<Message> messages;
    try {
        while (!bytes.empty()) {
            if (buffer_.size() < LENGTH_FIELD_SIZE) {
                const auto count = std::min(LENGTH_FIELD_SIZE - buffer_.size(), bytes.size());
                const auto part = bytes.first(count);
                buffer_.insert(buffer_.end(), part.begin(), part.end());
                bytes = bytes.subspan(count);
                if (buffer_.size() < LENGTH_FIELD_SIZE) {
                    break;
                }
            }

            std::uint32_t length = 0;
            for (std::size_t index = 0; index < LENGTH_FIELD_SIZE; ++index) {
                length = (length << 8) | std::to_integer<std::uint32_t>(buffer_[index]);
            }
            if (length == 0) {
                throw std::invalid_argument("Message body must contain a message type");
            }
            if (length > MAX_MESSAGE_SIZE) {
                throw std::invalid_argument("Declared message body exceeds maximum size");
            }

            const auto frame_size = LENGTH_FIELD_SIZE + static_cast<std::size_t>(length);
            const auto count = std::min(frame_size - buffer_.size(), bytes.size());
            const auto part = bytes.first(count);
            buffer_.insert(buffer_.end(), part.begin(), part.end());
            bytes = bytes.subspan(count);
            if (buffer_.size() < frame_size) {
                break;
            }

            messages.push_back(FrameDecoder::decode(buffer_));
            buffer_.clear();
        }
    } catch (...) {
        reset();
        throw;
    }
    return messages;
}

void FrameStreamDecoder::reset() noexcept {
    buffer_.clear();
}

void FrameStreamDecoder::finish() const {
    if (!buffer_.empty()) {
        throw std::invalid_argument("Incomplete frame at end of input");
    }
}

} // namespace rds::protocol
