add_library(response_constructor_standard_obj OBJECT
        standard/response_constructor.cpp
)

target_link_libraries(response_constructor_standard_obj
        PUBLIC
        kvdb_contracts
)

target_include_directories(response_constructor_standard_obj
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/standard"
)

target_compile_features(response_constructor_standard_obj
        PUBLIC
        cxx_std_20
)

set_target_properties(response_constructor_standard_obj PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)


add_library(response_constructor_standard SHARED
        $<TARGET_OBJECTS:response_constructor_standard_obj>
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