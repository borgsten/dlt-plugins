# Shared helpers for dlt-viewer plugins. dlt_add_plugin() links it.

add_library(plugin_utils STATIC
    include/plugin_utils/control_plugin.h
    include/plugin_utils/plugin_info.h
    include/plugin_utils/plugin_state.h
    src/control_plugin.cpp
    src/plugin_state.cpp
)

target_include_directories(plugin_utils PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(plugin_utils PUBLIC dlt::qdlt ${QT_PREFIX}::Widgets)

# Linked into plugin modules, which are shared libraries.
set_target_properties(plugin_utils PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(plugin_utils PRIVATE -Wall -Wextra)
endif()
