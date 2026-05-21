set(KVDB_TESTED_MODULE_SOURCES
        "${KVDB_ROOT_DIR}/modules/contracts/table_name.cpp"

        "${KVDB_ROOT_DIR}/modules/query_parser/standard/query_parser.cpp"
        "${KVDB_ROOT_DIR}/modules/engine/standard/engine.cpp"
        "${KVDB_ROOT_DIR}/modules/response_constructor/standard/response_constructor.cpp"
)

set(KVDB_TESTED_MODULE_HEADERS
        "${KVDB_ROOT_DIR}/modules/contracts/err.h"
        "${KVDB_ROOT_DIR}/modules/contracts/i_modules.h"
        "${KVDB_ROOT_DIR}/modules/contracts/table_name.h"

        "${KVDB_ROOT_DIR}/modules/contracts/cmds/cmd_values.h"
        "${KVDB_ROOT_DIR}/modules/contracts/cmds/cmd_exec_results.h"
        "${KVDB_ROOT_DIR}/modules/contracts/cmds/cmd_dtos.h"
        "${KVDB_ROOT_DIR}/modules/contracts/cmds/cmd_parse_results.h"
)

set(KVDB_TESTED_MODULE_INCLUDE_DIRS
        "${KVDB_ROOT_DIR}/modules/contracts"
        "${KVDB_ROOT_DIR}/modules/contracts/cmds"

        "${KVDB_ROOT_DIR}/modules/query_parser/standard"
        "${KVDB_ROOT_DIR}/modules/engine/standard"
        "${KVDB_ROOT_DIR}/modules/response_constructor/standard"
)

add_executable(kvdb_unit_tests
        ${KVDB_TESTED_MODULE_SOURCES}
        ${KVDB_TESTED_MODULE_HEADERS}

        unit/contracts_tests.cpp
        unit/table_name_tests.cpp
        unit/query_parser_tests.cpp
        unit/engine_tests.cpp
        unit/response_constructor_tests.cpp
)

target_link_libraries(kvdb_unit_tests
        PRIVATE
        Catch2::Catch2WithMain
)

target_include_directories(kvdb_unit_tests
        PRIVATE
        ${KVDB_TESTED_MODULE_INCLUDE_DIRS}
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
            ${KVDB_TESTED_MODULE_SOURCES}
            ${KVDB_TESTED_MODULE_HEADERS}

            ${SCENARIO_SOURCE}
    )

    target_include_directories(${SCENARIO_TARGET}
            PRIVATE
            ${KVDB_TESTED_MODULE_INCLUDE_DIRS}
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