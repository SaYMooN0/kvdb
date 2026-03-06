cmake_minimum_required(VERSION 3.16)

project(kvdb_server)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(contracts)
add_subdirectory(query_parser)
add_subdirectory(engine)
add_subdirectory(response_constructor)
add_subdirectory(server)