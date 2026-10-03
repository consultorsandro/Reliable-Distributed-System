#pragma once

namespace rds::transport {

// On Windows, declare before socket owners so reverse destruction order closes
// sockets first. Each instance owns one successful WSAStartup reference.
// On POSIX, this type is a no-op retained for source compatibility.
class WinsockRuntime {
public:
    // On Windows, throws std::system_error if WSAStartup fails. On POSIX, no-op.
    WinsockRuntime();
    ~WinsockRuntime() noexcept;

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
    WinsockRuntime(WinsockRuntime&&) = delete;
    WinsockRuntime& operator=(WinsockRuntime&&) = delete;
};

} // namespace rds::transport
