add_executable(dlt_test_source dlt_test_source.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dlt_test_source PRIVATE -Wall -Wextra)
endif()
