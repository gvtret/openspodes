cmake_minimum_required(VERSION 3.16)
project(openspodes VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Library
add_library(openspodes STATIC
    src/codec/codec.c
    src/codec/serialize.c
    src/codec/ic_serialize.c
    src/ic/ic_common.c
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
    src/ic/profile_generic.c
    src/ic/image_transfer.c
    src/ic/activity_calendar.c
    src/ic/register_monitor.c
    src/ic/single_action_schedule.c
    src/ic/ipv4_setup.c
    src/ic/tcp_udp_setup.c
    src/ic/profile_filter.c
    src/ic/arbitrator.c
    src/ic/demand_register.c
    src/ic/status_mapping.c
    src/ic/utility_tables.c
    src/ic/compact_data.c
    src/ic/iec_hdlc_setup.c
    src/ic/iec_local_port_setup.c
    src/ic/data_protection.c
    src/ic/push_setup.c
    src/ic/association_ln.c
    src/ic/mac_address.c
    src/ic/gprs_modem.c
    src/ic/gsm_diagnostic.c
    src/ic/ipv6_setup.c
    src/ic/register_table.c
    src/ic/parameter_monitor.c
    src/ic/mbus_slave.c
    src/ic/mbus_slave_port_setup.c
    src/ic/spodus_helpers.c
    src/ic/profile_data_filter.c
    src/ic/table_manager.c
    src/transport/transport.c
    src/service/service.c
    src/service/xdms.c
    src/service/notification.c
    src/service/gbt.c
    src/service/initiate.c
    src/security/security.c
    src/security/streebog/gost3411-2012-core.c
    src/security/streebog_wrap.c
    src/security/kuznyechik.c
    src/security/gost3410.c
    src/security/general_ciphering.c
    src/client/client.c
    src/server/server.c
)

target_include_directories(openspodes PUBLIC src)
target_include_directories(openspodes PRIVATE src/security/streebog)

option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
	add_compile_options(--coverage -O0 -g)
	add_link_options(--coverage)
endif()

# Tests
include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
    GIT_TAG        cmocka-1.1.7
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WITH_CMOCKERY_SUPPORT OFF CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UNIT_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)

enable_testing()
add_executable(openspodes_test tests/test_core.c)
target_include_directories(openspodes_test PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test PRIVATE openspodes cmocka)
add_test(NAME openspodes_test COMMAND openspodes_test)

add_executable(openspodes_test_golden tests/test_codec_golden.c)
target_include_directories(openspodes_test_golden PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_golden PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_golden COMMAND openspodes_test_golden)

add_executable(openspodes_test_errors tests/test_errors.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_errors PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_errors PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_errors COMMAND openspodes_test_errors)

add_executable(openspodes_test_ic tests/test_ic_smoke.c)
target_include_directories(openspodes_test_ic PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_ic PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_ic COMMAND openspodes_test_ic)

add_executable(openspodes_test_integration tests/test_integration.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_integration PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_integration PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_integration COMMAND openspodes_test_integration)

add_executable(openspodes_test_phase0 tests/test_phase0.c)
target_include_directories(openspodes_test_phase0 PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase0 PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_phase0 COMMAND openspodes_test_phase0)

add_executable(gen_ic_golden_vectors tools/gen_ic_golden_vectors.c tests/fixtures/ic_spodes_fixtures.c)
target_include_directories(gen_ic_golden_vectors PRIVATE tests tests/fixtures)
target_link_libraries(gen_ic_golden_vectors PRIVATE openspodes)

add_executable(openspodes_test_phase1 tests/test_phase1.c tests/fixtures/ic_spodes_fixtures.c)
target_include_directories(openspodes_test_phase1 PRIVATE tests tests/fixtures ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase1 PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_phase1 COMMAND openspodes_test_phase1)

add_executable(openspodes_test_phase2 tests/test_phase2.c tests/mock_transport.c)
target_include_directories(openspodes_test_phase2 PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase2 PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_phase2 COMMAND openspodes_test_phase2)

add_executable(openspodes_test_gost tests/test_gost_crypto.c)
target_include_directories(openspodes_test_gost PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_gost PRIVATE openspodes cmocka)
add_test(NAME openspodes_test_gost COMMAND openspodes_test_gost)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
	add_executable(openspodes_test_security tests/test_security_glo.c tests/mock_crypto.c tests/real_crypto.c)
	target_include_directories(openspodes_test_security PRIVATE ${cmocka_SOURCE_DIR}/include tests)
	target_link_libraries(openspodes_test_security PRIVATE openspodes cmocka OpenSSL::Crypto)
	target_compile_definitions(openspodes_test_security PRIVATE OSP_HAVE_OPENSSL_GCM=1)
	add_test(NAME openspodes_test_security COMMAND openspodes_test_security)

	target_sources(openspodes_test_integration PRIVATE tests/real_crypto.c)
	target_link_libraries(openspodes_test_integration PRIVATE OpenSSL::Crypto)
	target_compile_definitions(openspodes_test_integration PRIVATE OSP_HAVE_OPENSSL_GCM=1)
	# ECDSA HLS (mech 7) uses OpenSSL via mock_crypto_init_real_gost_ecdsa in tests
endif()

add_executable(openspodes_loopback_cli
	examples/loopback_cli.c
	tests/mock_transport.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_loopback_cli PRIVATE tests)
target_link_libraries(openspodes_loopback_cli PRIVATE openspodes)
add_test(NAME openspodes_loopback_cli_demo COMMAND openspodes_loopback_cli demo)

add_executable(openspodes_tcp_server
	examples/tcp_server.c
	examples/tcp_socket.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_tcp_server PRIVATE tests examples)
target_link_libraries(openspodes_tcp_server PRIVATE openspodes)

add_executable(openspodes_tcp_client
	examples/tcp_client.c
	examples/tcp_socket.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_tcp_client PRIVATE tests examples)
target_link_libraries(openspodes_tcp_client PRIVATE openspodes)

if(ENABLE_COVERAGE)
	find_package(Python3 COMPONENTS Interpreter REQUIRED)
	add_custom_target(coverage
		COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/coverage_report.py
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		DEPENDS openspodes_test openspodes_test_golden openspodes_test_ic openspodes_test_errors openspodes_test_integration openspodes_test_phase0 openspodes_test_phase1
		COMMENT "Run tests and print gcov coverage report"
	)
endif()
