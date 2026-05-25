add_library(engine_standard_obj OBJECT
        standard/engine.cpp
        standard/engine_values.cpp
        standard/engine_persistence.cpp
)

target_link_libraries(engine_standard_obj
        PUBLIC
        kvdb_contracts
)

target_include_directories(engine_standard_obj
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/standard"
)

target_compile_features(engine_standard_obj
        PUBLIC
        cxx_std_20
)

set_target_properties(engine_standard_obj PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)


add_library(engine_standard SHARED
        $<TARGET_OBJECTS:engine_standard_obj>
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