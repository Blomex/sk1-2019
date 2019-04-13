cmake_minimum_required(VERSION 3.1)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")
add_executable(netstore_client klient.c err.c common_structs.h)
add_executable(netstore_server serwer.c err.c common_structs.h)