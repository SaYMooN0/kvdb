add_executable(db_instance
        main.cpp
        dll_loader.cpp
)

target_link_libraries(db_instance
        PRIVATE
        kvdb_contracts
)

if (UNIX AND NOT APPLE)
    target_link_libraries(db_instance
            PRIVATE
            dl
            pthread
    )
endif()

target_compile_features(db_instance
        PRIVATE
        cxx_std_20
)

set_target_properties(db_instance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_DIST_DIR}"
)