# No Qt or dlt-viewer dependencies; also builds on its own:
#   cmake -S tools/dlt-filter-proxy -B build/proxy
cmake_minimum_required(VERSION 3.16)
project(dlt-filter-proxy LANGUAGES CXX)

if(NOT TARGET dlt_proxy)
    add_subdirectory(../../libs/dlt_proxy dlt_proxy)
endif()

add_executable(dlt-filter-proxy main.cpp)
target_link_libraries(dlt-filter-proxy PRIVATE dlt_proxy)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dlt-filter-proxy PRIVATE -Wall -Wextra)
endif()
