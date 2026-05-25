set(KVDB_TESTED_MODULE_LIBS
        kvdb_contracts
        query_parser_standard_obj
        engine_standard_obj
        response_constructor_standard_obj
)

add_executable(kvdb_unit_tests
        unit/contracts_tests.cpp
        unit/table_name_tests.cpp
        unit/query_parser_tests.cpp
        unit/engine_tests.cpp
        unit/response_constructor_tests.cpp
)

target_link_libraries(kvdb_unit_tests
        PRIVATE
        Catch2::Catch2WithMain
        ${KVDB_TESTED_MODULE_LIBS}
)

target_compile_features(kvdb_unit_tests
        PRIVATE
        cxx_std_20
)

set_target_properties(kvdb_unit_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_TESTS_OUTPUT_DIR}/unit"
)

add_test(
        NAME kvdb_unit_tests
        COMMAND kvdb_unit_tests
)

file(GLOB KVDB_SCENARIO_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.cpp"
)

foreach(SCENARIO_SOURCE ${KVDB_SCENARIO_SOURCES})
    get_filename_component(SCENARIO_TARGET ${SCENARIO_SOURCE} NAME_WE)

    add_executable(${SCENARIO_TARGET}
            ${SCENARIO_SOURCE}
    )

    target_link_libraries(${SCENARIO_TARGET}
            PRIVATE
            ${KVDB_TESTED_MODULE_LIBS}
    )

    target_compile_features(${SCENARIO_TARGET}
            PRIVATE
            cxx_std_20
    )

    set_target_properties(${SCENARIO_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${KVDB_TESTS_OUTPUT_DIR}/scenarios"
    )

    add_test(
            NAME ${SCENARIO_TARGET}
            COMMAND ${SCENARIO_TARGET}
    )
endforeach()