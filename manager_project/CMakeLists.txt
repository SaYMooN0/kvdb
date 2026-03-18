add_executable(kvdb_manager
        main.cpp
)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" KVDB_SOURCE_DIR_CMAKE)
file(TO_CMAKE_PATH "${KVDB_DIST_DIR}" KVDB_DIST_DIR_CMAKE)

target_compile_definitions(kvdb_manager
        PRIVATE
        KVDB_SOURCE_DIR="${KVDB_SOURCE_DIR_CMAKE}"
        KVDB_DIST_DIR="${KVDB_DIST_DIR_CMAKE}"
)

set_target_properties(kvdb_manager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_DIST_DIR}"
)