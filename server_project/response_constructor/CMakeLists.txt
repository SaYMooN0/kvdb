if(NOT DEFINED RESPONSE_IMPL)
    set(RESPONSE_IMPL standard)
endif()

add_library(response_constructor
        impl/${RESPONSE_IMPL}/response_constructor.cpp
)

target_link_libraries(response_constructor PUBLIC contracts)

target_include_directories(response_constructor PUBLIC .)