add_library(query_parser_standard SHARED
        standard/query_parser.cpp
)

target_link_libraries(query_parser_standard
        PRIVATE
        kvdb_contracts
)

set_target_properties(query_parser_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "query_parser_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)

add_library(query_parser_ru SHARED
        ru/query_parser.cpp
)

target_link_libraries(query_parser_ru
        PRIVATE
        kvdb_contracts
)

set_target_properties(query_parser_ru PROPERTIES
        PREFIX ""
        OUTPUT_NAME "query_parser_ru"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)