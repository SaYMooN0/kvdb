add_library(access_interface_standard SHARED
        standard/access_interface.cpp
)

target_link_libraries(access_interface_standard
        PRIVATE
        kvdb_contracts
)

set_target_properties(access_interface_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "access_interface_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)