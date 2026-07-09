cmake_minimum_required(VERSION 3.16)
project(openspodes VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library
add_library(openspodes STATIC
    src/codec/codec.c
    src/codec/serialize.c
    src/server/dispatcher.c
    src/ic/data.c
)

target_include_directories(openspodes PUBLIC src)

# Tests
enable_testing()
add_executable(openspodes_test tests/test_core.c)
target_link_libraries(openspodes_test PRIVATE openspodes)
add_test(NAME openspodes_test COMMAND openspodes_test)
