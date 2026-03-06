cmake_minimum_required(VERSION 3.16)
project(kvdb)

set(CMAKE_CXX_STANDARD 20)

add_executable(kvdb_manager
        main.cpp
)

target_compile_definitions(kvdb_manager PRIVATE KVDB_CMAKE_PATH="${CMAKE_COMMAND}")
target_compile_definitions(kvdb_manager PRIVATE KVDB_SOURCE_DIR="${CMAKE_SOURCE_DIR}")