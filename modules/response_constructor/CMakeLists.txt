add_library(response_constructor_standard SHARED
        standard/response_constructor.cpp
)

target_link_libraries(response_constructor_standard
        PRIVATE
        kvdb_contracts
)

set_target_properties(response_constructor_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "response_constructor_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)