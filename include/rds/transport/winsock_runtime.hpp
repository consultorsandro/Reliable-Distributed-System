#pragma once

namespace rds::transport {

// Declare before socket owners so reverse destruction order closes sockets first.
// Each instance owns one successful WSAStartup reference; no shared global owner.
class WinsockRuntime {
public:
    // Throws std::system_error with the WSAStartup result on initialization failure.
    WinsockRuntime();
    ~WinsockRuntime() noexcept;

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
    WinsockRuntime(WinsockRuntime&&) = delete;
    WinsockRuntime& operator=(WinsockRuntime&&) = delete;
};

} // namespace rds::transport
