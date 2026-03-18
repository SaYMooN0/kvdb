add_library(server_standard SHARED
        standard/server.cpp
)

target_link_libraries(server_standard
        PRIVATE
        kvdb_contracts
)

set_target_properties(server_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "server_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)