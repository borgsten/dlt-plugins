# dlt_add_plugin(<name> <sources>...)

set(DLT_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/dlt-viewer/plugins" CACHE PATH
    "Directory plugins are installed to")

function(dlt_add_plugin NAME)
    add_library(${NAME} MODULE ${ARGN})

    set_target_properties(${NAME} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/plugins")

    target_include_directories(${NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${NAME} PRIVATE plugin_utils dlt::qdlt ${QT_PREFIX}::Widgets)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${NAME} PRIVATE -Wall -Wextra)
    endif()
    # Qt and qdlt pull in libraries (e.g. OpenGL) for their headers only;
    # record just the ones the plugin uses.
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(${NAME} PRIVATE "LINKER:--as-needed")
    endif()

    install(TARGETS ${NAME}
        LIBRARY DESTINATION "${DLT_PLUGIN_INSTALL_DIR}"
        RUNTIME DESTINATION "${DLT_PLUGIN_INSTALL_DIR}")
endfunction()
