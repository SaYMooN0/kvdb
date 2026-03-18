add_library(kvdb_contracts STATIC
        table_name.cpp
)

target_include_directories(kvdb_contracts
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(kvdb_contracts PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)