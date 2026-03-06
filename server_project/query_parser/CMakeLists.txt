if(NOT DEFINED PARSER_IMPL)
    set(PARSER_IMPL standard)
endif()

add_library(query_parser
        impl/${PARSER_IMPL}/query_parser.cpp
)

target_link_libraries(query_parser PUBLIC contracts)

target_include_directories(query_parser PUBLIC .)