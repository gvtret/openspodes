cmake_minimum_required(VERSION 3.16)
project(openspodes VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library
add_library(openspodes STATIC
    src/codec/codec.c
    src/codec/serialize.c
    src/server/dispatcher.c
    src/ic/data.c
    src/ic/register.c
    src/ic/extended_register.c
    src/ic/clock.c
    src/ic/security_setup.c
    src/ic/disconnect_control.c
    src/ic/limiter.c
    src/ic/schedule.c
    src/ic/script_table.c
    src/ic/special_days.c
    src/ic/register_activation.c
    src/ic/sap_assignment.c
    src/transport/transport.c
    src/service/service.c
    src/security/security.c
)

target_include_directories(openspodes PUBLIC src)

# Tests
find_library(CMOCKA_LIBRARY cmocka)
find_path(CMOCKA_INCLUDE_DIR cmocka.h)

enable_testing()
add_executable(openspodes_test tests/test_core.c)
target_include_directories(openspodes_test PRIVATE ${CMOCKA_INCLUDE_DIR})
target_link_libraries(openspodes_test PRIVATE openspodes ${CMOCKA_LIBRARY})
add_test(NAME openspodes_test COMMAND openspodes_test)
