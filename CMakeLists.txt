cmake_minimum_required(VERSION 3.20)

project(ReliableDistributedSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rds_protocol
    src/protocol/framing.cpp
    src/protocol/frame_stream_decoder.cpp
)
target_include_directories(rds_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    add_library(rds_transport
        src/transport/socket_handle.cpp
        src/transport/winsock_runtime.cpp
        src/transport/tcp.cpp
    )
    target_include_directories(rds_transport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(rds_transport PUBLIC ws2_32)
    if(MSVC)
        target_compile_options(rds_transport PRIVATE /EHsc)
    endif()
endif()

add_executable(rds_client src/client/main.cpp)
add_executable(rds_server src/server/main.cpp)

if(MSVC)
    target_compile_options(rds_protocol PRIVATE /EHsc)
    target_compile_options(rds_client PRIVATE /EHsc)
    target_compile_options(rds_server PRIVATE /EHsc)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
