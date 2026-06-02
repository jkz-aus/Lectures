cmake_minimum_required(VERSION 3.22)
project(list_client_binary)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SOURCES
    main.cpp
    src/socket_utils.cpp
    src/types.cpp
    src/remote_list_stub.cpp
    src/remote_list.cpp
)

add_executable(list_client_binary ${SOURCES})
