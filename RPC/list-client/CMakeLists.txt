cmake_minimum_required(VERSION 3.22)
project(key_value_client)

set(CMAKE_CXX_STANDARD 20)

# Include directories for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    main.cpp
    src/socket_utils.cpp
    src/types.cpp
    src/remote_list_stub.cpp
    src/remote_list.cpp
)

add_executable(key_value_client ${SOURCES})
