cmake_minimum_required(VERSION 3.22)
project(name_server_binary)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(name_server_binary
    src/main.cpp
    src/RequestHandlers.cpp
    src/ResponseHandlers.cpp
)
