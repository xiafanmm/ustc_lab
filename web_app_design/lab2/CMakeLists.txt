cmake_minimum_required(VERSION 3.10)
project(chatroom CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# common 库
add_library(common STATIC
    common/common.cpp
)
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# server
add_executable(server
    server/server.cpp
)
target_link_libraries(server
    common
    pthread
)

# client
add_executable(client
    client/client.cpp
)
target_link_libraries(client
    common
    pthread
)