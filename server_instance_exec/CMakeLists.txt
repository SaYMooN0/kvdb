if (WIN32)
    add_executable(server_instance_exec
            main.cpp
            dll_loader.cpp
    )
else()
    message(FATAL_ERROR "server_instance_exec is currently implemented only for Windows")
endif()

target_link_libraries(server_instance_exec
        PRIVATE
        kvdb_contracts
)

set_target_properties(server_instance_exec PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KVDB_DIST_DIR}"
)