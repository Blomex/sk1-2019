cmake_minimum_required(VERSION 3.1)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_STANDARD 11)
add_executable(netstore_client netstore-client.c err.c common_structs.h)
add_executable(snippet snippet.c err.c)
add_executable(netstore_server netstore-server.c err.c common_structs.h)