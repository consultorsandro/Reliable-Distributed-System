#include "rds/transport/winsock_runtime.hpp"

namespace rds::transport {

// POSIX sockets require no process-wide initialization. This no-op preserves
// source compatibility with application code that establishes runtime lifetime
// before declaring socket owners on Windows.
WinsockRuntime::WinsockRuntime() = default;

WinsockRuntime::~WinsockRuntime() noexcept = default;

} // namespace rds::transport
