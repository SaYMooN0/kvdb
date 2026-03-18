add_library(engine_standard SHARED
        standard/engine.cpp
)

target_link_libraries(engine_standard
        PRIVATE
        kvdb_contracts
)

set_target_properties(engine_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "engine_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)