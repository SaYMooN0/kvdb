add_library(access_interface_standard SHARED
        standard/access_interface.cpp
)

target_link_libraries(access_interface_standard
        PRIVATE
        kvdb_contracts
)

if (WIN32)
    target_link_libraries(access_interface_standard
            PRIVATE
            ws2_32
    )
endif()

target_include_directories(access_interface_standard
        PRIVATE
        "${CMAKE_SOURCE_DIR}/third_party/boost"
)

target_compile_features(access_interface_standard
        PRIVATE
        cxx_std_20
)

set_target_properties(access_interface_standard PROPERTIES
        PREFIX ""
        OUTPUT_NAME "access_interface_standard"
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${KVDB_MODULES_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${KVDB_LIB_OUTPUT_DIR}"
)