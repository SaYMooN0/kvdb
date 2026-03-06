if(NOT DEFINED SERVER_IMPL)
    set(SERVER_IMPL standard)
endif()

add_executable(server
        impl/${SERVER_IMPL}/server.cpp
)

target_link_libraries(server
        query_parser
        engine
        response_constructor
        contracts
)