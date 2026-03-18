cmake_minimum_required(VERSION 3.20)

project(kvdb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KVDB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(KVDB_MODULES_OUTPUT_DIR "${KVDB_DIST_DIR}/modules")
set(KVDB_LIB_OUTPUT_DIR "${KVDB_DIST_DIR}/lib")

add_subdirectory(modules)
add_subdirectory(server_instance_exec)
add_subdirectory(manager_project)