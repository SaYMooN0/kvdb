if (WIN32)
    add_executable(db_instance
            main.cpp
            dll_loader.cpp
    )
else()
    message(FATAL_ERROR "db_instance is currently implemented only for Windows")
endif()

target_link_libraries(db_instance
        PRIVATE
        kvdb_contracts
)

set_target_properties(db_instance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_DIST_DIR}"
)