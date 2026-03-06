if(NOT DEFINED ENGINE_IMPL)
    set(ENGINE_IMPL standard)
endif()

add_library(engine
        impl/${ENGINE_IMPL}/engine.cpp
)

target_link_libraries(engine PUBLIC contracts)

target_include_directories(engine PUBLIC .)