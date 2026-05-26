add_library(kvdb_contracts STATIC
        table_name.cpp
)

target_include_directories(kvdb_contracts
        PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmds"
)

target_compile_features(kvdb_contracts
        PUBLIC
        cxx_std_20
)

set_target_properties(kvdb_contracts PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)