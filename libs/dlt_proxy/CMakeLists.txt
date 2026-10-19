# The DLT filter proxy, used by tools/dlt-filter-proxy and the dltproxy
# plugin. No Qt or dlt-viewer dependencies.

add_library(dlt_proxy STATIC
    include/dlt_proxy/proxy.h
    src/proxy.cpp
)

target_include_directories(dlt_proxy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(dlt_proxy PUBLIC cxx_std_20)

# Linked into plugin modules, which are shared libraries.
set_target_properties(dlt_proxy PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dlt_proxy PRIVATE -Wall -Wextra)
endif()
