#include "rds/transport/winsock_runtime.hpp"

#include <winsock2.h>

#include <cstdio>
#include <system_error>

namespace rds::transport {
namespace {

void cleanup() noexcept {
    if (::WSACleanup() == SOCKET_ERROR) {
        const auto error = ::WSAGetLastError();
        std::fprintf(stderr, "WSACleanup failed (Winsock error %d)\n", error);
    }
}

} // namespace

WinsockRuntime::WinsockRuntime() {
    WSADATA data{};
    const int result = ::WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        throw std::system_error(result, std::system_category(), "WSAStartup(2.2)");
    }
    if (data.wVersion != MAKEWORD(2, 2)) {
        cleanup();
        throw std::system_error(WSAVERNOTSUPPORTED, std::system_category(),
                                "WSAStartup did not negotiate Winsock 2.2");
    }
}

WinsockRuntime::~WinsockRuntime() noexcept { cleanup(); }

} // namespace rds::transport
