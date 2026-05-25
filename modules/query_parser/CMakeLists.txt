add_library(query_parser_standard_obj OBJECT
        standard/query_parser.cpp
        standard/query_lexer.cpp
)

target_link_libraries(query_parser_standard_obj
        PUBLIC
        kvdb_contracts
)

target_include_directories(query_parser_standard_obj
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/standard"
)

target_compile_features(query_parser_standard_obj
        PUBLIC
        cxx_std_20
)

set_target_properties(query_parser_standard_obj PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)


add_library(query_parser_standard SHARED
        $<TARGET_OBJECTS:query_parser_standard_obj>
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