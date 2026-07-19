cmake_minimum_required(VERSION 3.16)
project(openspodes VERSION 2.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
	add_compile_options(--coverage -O0 -g)
	add_link_options(--coverage)
endif()

option(ENABLE_ASAN "Build with AddressSanitizer + UBSan" OFF)
if(ENABLE_ASAN)
	add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
	add_link_options(-fsanitize=address,undefined)
endif()

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
    src/transport/hdlc_session.c
    src/service/service.c
    src/service/xdms.c
    src/service/xdms_selective.c
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
    src/spodus/meter_registry.c
    src/spodus/direct_channel.c
    src/spodus/channel_list.c
    src/spodus/discovered.c
    src/spodus/access_policy.c
    src/spodus/tasks.c
    src/spodus/concentrator.c
    src/spodus/spodus_server.c
)

target_include_directories(openspodes PUBLIC src)
target_include_directories(openspodes PRIVATE src/security/streebog)

# Hardening flags
target_compile_options(openspodes PRIVATE -Wall -Wextra -Werror -fstack-protector-strong)
target_compile_definitions(openspodes PRIVATE _FORTIFY_SOURCE=2)

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
# All tests must agree with the library on OSP_MAX_OBJECTS to avoid ABI mismatch
set(YB_OSP_MAX_OBJECTS OSP_MAX_OBJECTS=320)

add_executable(openspodes_test tests/test_core.c tests/mock_crypto.c)
target_include_directories(openspodes_test PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test COMMAND openspodes_test)

add_executable(openspodes_test_golden tests/test_codec_golden.c)
target_include_directories(openspodes_test_golden PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_golden PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_golden PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_golden COMMAND openspodes_test_golden)

add_executable(openspodes_test_errors tests/test_errors.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_errors PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_errors PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_errors PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_errors COMMAND openspodes_test_errors)

add_executable(openspodes_test_ic tests/test_ic_smoke.c)
target_include_directories(openspodes_test_ic PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_ic PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_ic PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_ic COMMAND openspodes_test_ic)

add_executable(openspodes_test_integration tests/test_integration.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_integration PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_integration PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_integration PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_integration COMMAND openspodes_test_integration)

add_executable(openspodes_test_phase0 tests/test_phase0.c)
target_include_directories(openspodes_test_phase0 PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase0 PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_phase0 PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_phase0 COMMAND openspodes_test_phase0)

add_executable(gen_ic_golden_vectors tools/gen_ic_golden_vectors.c tests/fixtures/ic_spodes_fixtures.c)
target_include_directories(gen_ic_golden_vectors PRIVATE tests tests/fixtures)
target_link_libraries(gen_ic_golden_vectors PRIVATE openspodes)

add_executable(openspodes_test_phase1 tests/test_phase1.c tests/fixtures/ic_spodes_fixtures.c)
target_include_directories(openspodes_test_phase1 PRIVATE tests tests/fixtures ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase1 PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_phase1 PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_phase1 COMMAND openspodes_test_phase1)

add_executable(openspodes_test_phase2 tests/test_phase2.c tests/mock_transport.c)
target_include_directories(openspodes_test_phase2 PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(openspodes_test_phase2 PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_phase2 PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_phase2 COMMAND openspodes_test_phase2)

add_executable(openspodes_test_spodus tests/test_spodus_concentrator.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_spodus PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_spodus PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_spodus PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_spodus COMMAND openspodes_test_spodus)

add_executable(openspodes_test_gost tests/test_gost_crypto.c)
target_include_directories(openspodes_test_gost PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_gost PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_gost PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_gost COMMAND openspodes_test_gost)

add_executable(openspodes_test_hdlc tests/test_hdlc_session.c tests/mock_transport.c)
target_include_directories(openspodes_test_hdlc PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_hdlc PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_hdlc PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_hdlc COMMAND openspodes_test_hdlc)

add_executable(openspodes_test_framing tests/test_hdlc_wrapper_framing.c tests/mock_transport.c tests/mock_crypto.c)
target_include_directories(openspodes_test_framing PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_framing PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_framing PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_framing COMMAND openspodes_test_framing)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
	add_executable(openspodes_test_security tests/test_security_glo.c tests/mock_crypto.c tests/real_crypto.c)
	target_include_directories(openspodes_test_security PRIVATE ${cmocka_SOURCE_DIR}/include tests)
	target_link_libraries(openspodes_test_security PRIVATE openspodes cmocka OpenSSL::Crypto)
	target_compile_definitions(openspodes_test_security PRIVATE OSP_HAVE_OPENSSL_GCM=1 ${YB_OSP_MAX_OBJECTS})
	add_test(NAME openspodes_test_security COMMAND openspodes_test_security)

	target_sources(openspodes_test_integration PRIVATE tests/real_crypto.c)
	target_link_libraries(openspodes_test_integration PRIVATE OpenSSL::Crypto)
	target_compile_definitions(openspodes_test_integration PRIVATE OSP_HAVE_OPENSSL_GCM=1)

	target_sources(openspodes_test_errors PRIVATE tests/real_crypto.c)
	target_link_libraries(openspodes_test_errors PRIVATE OpenSSL::Crypto)
	target_compile_definitions(openspodes_test_errors PRIVATE OSP_HAVE_OPENSSL_GCM=1)

	# ECDSA HLS (mech 7) uses OpenSSL via mock_crypto_init_real_gost_ecdsa in tests

	target_sources(openspodes_test PRIVATE tests/real_crypto.c)
	target_link_libraries(openspodes_test PRIVATE OpenSSL::Crypto)
	target_compile_definitions(openspodes_test PRIVATE OSP_HAVE_OPENSSL_GCM=1)
endif()

add_executable(openspodes_loopback_cli
	examples/loopback_cli.c
	tests/mock_transport.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_loopback_cli PRIVATE tests)
target_link_libraries(openspodes_loopback_cli PRIVATE openspodes)
target_compile_definitions(openspodes_loopback_cli PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_loopback_cli_demo COMMAND openspodes_loopback_cli demo)

add_executable(openspodes_push_listener
	examples/push_listener.c
	tests/mock_transport.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_push_listener PRIVATE tests)
target_link_libraries(openspodes_push_listener PRIVATE openspodes)
target_compile_definitions(openspodes_push_listener PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_push_listener_demo COMMAND openspodes_push_listener)

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

add_executable(openspodes_linux_demo
	examples/linux_demo.c
	examples/linux_hal.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_linux_demo PRIVATE tests examples)
target_link_libraries(openspodes_linux_demo PRIVATE openspodes)

add_executable(openspodes_spodus_demo
	examples/spodus_demo.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_spodus_demo PRIVATE tests examples)
target_link_libraries(openspodes_spodus_demo PRIVATE openspodes)

add_executable(openspodes_test_types
	tests/test_types.c
)
target_include_directories(openspodes_test_types PRIVATE tests)
target_link_libraries(openspodes_test_types PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_types PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_types COMMAND openspodes_test_types)

add_executable(openspodes_serial_client
	examples/serial_client.c
	examples/serial_transport.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_serial_client PRIVATE tests examples)
target_link_libraries(openspodes_serial_client PRIVATE openspodes)

add_executable(openspodes_serial_server
	examples/serial_server.c
	examples/serial_transport.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_serial_server PRIVATE tests examples)
target_link_libraries(openspodes_serial_server PRIVATE openspodes)

add_executable(openspodes_bench
	tests/test_performance.c
	tests/mock_crypto.c
)
target_include_directories(openspodes_bench PRIVATE tests)
target_link_libraries(openspodes_bench PRIVATE openspodes)

if(OpenSSL_FOUND)
	target_sources(openspodes_bench PRIVATE tests/real_crypto.c)
	target_link_libraries(openspodes_bench PRIVATE OpenSSL::Crypto)
	target_compile_definitions(openspodes_bench PRIVATE OSP_HAVE_OPENSSL_GCM=1)
endif()

if(ENABLE_COVERAGE)
	find_package(Python3 COMPONENTS Interpreter REQUIRED)
	add_custom_target(coverage
		COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/coverage_report.py
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		DEPENDS openspodes_test openspodes_test_golden openspodes_test_ic openspodes_test_errors openspodes_test_integration openspodes_test_phase0 openspodes_test_phase1 openspodes_test_spodus openspodes_test_gost openspodes_test_hdlc openspodes_test_framing openspodes_test_security
		yb_test_hdlc_frame yb_test_hdlc_address yb_test_hdlc_ndm2nrm yb_test_hdlc_info yb_test_hdlc_ndmop
		yb_test_appl_open yb_test_appl_data yb_test_appl_rel yb_test_appl_sec yb_test_cosem_objs yb_test_symsec_0
		COMMENT "Run tests and print gcov coverage report"
	)
endif()

add_executable(openspodes_test_thread
    tests/test_thread_safety.c
)
target_include_directories(openspodes_test_thread PRIVATE tests)
target_link_libraries(openspodes_test_thread PRIVATE openspodes cmocka pthread)
target_compile_definitions(openspodes_test_thread PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_thread COMMAND openspodes_test_thread)

add_executable(openspodes_test_yellowbook_e2e
    tests/test_yellowbook_e2e.c
    tests/mock_transport.c
    tests/mock_crypto.c
    tests/yb_helpers.c
)
target_include_directories(openspodes_test_yellowbook_e2e PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_yellowbook_e2e PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_yellowbook_e2e PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_yellowbook_e2e COMMAND openspodes_test_yellowbook_e2e)

add_executable(openspodes_test_yellowbook_hdlc
    tests/test_yellowbook_hdlc.c
    tests/mock_transport.c
    tests/mock_crypto.c
    tests/yb_helpers.c
)
target_include_directories(openspodes_test_yellowbook_hdlc PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_yellowbook_hdlc PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_yellowbook_hdlc PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_yellowbook_hdlc COMMAND openspodes_test_yellowbook_hdlc)

add_executable(openspodes_test_yellowbook_appl
    tests/test_yellowbook_appl.c
    tests/mock_transport.c
    tests/mock_crypto.c
    tests/yb_helpers.c
)
target_include_directories(openspodes_test_yellowbook_appl PRIVATE ${cmocka_SOURCE_DIR}/include tests)
target_link_libraries(openspodes_test_yellowbook_appl PRIVATE openspodes cmocka)
target_compile_definitions(openspodes_test_yellowbook_appl PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME openspodes_test_yellowbook_appl COMMAND openspodes_test_yellowbook_appl)

# ── Yellow Book Conformance Tests ──────────────────────────────────────────
set(YB_SOURCES tests/yb_helpers.c tests/mock_transport.c tests/mock_crypto.c)
set(YB_INCLUDES ${cmocka_SOURCE_DIR}/include tests)

# ATS_DL V5: Data Link Layer (5 targets)
foreach(name hdlc_frame hdlc_address hdlc_ndm2nrm hdlc_info hdlc_ndmop)
	add_executable(yb_test_${name} tests/test_yb_ats_dl_${name}.c tests/mock_transport.c tests/mock_crypto.c)
	target_include_directories(yb_test_${name} PRIVATE ${YB_INCLUDES})
	target_link_libraries(yb_test_${name} PRIVATE openspodes cmocka)
	target_compile_definitions(yb_test_${name} PRIVATE ${YB_OSP_MAX_OBJECTS})
	add_test(NAME yb_test_${name} COMMAND yb_test_${name})
endforeach()

# ATS_AL: Application Layer (6 targets, 5 need yb_helpers)
foreach(name appl_open appl_data appl_rel appl_sec cosem_objs)
	add_executable(yb_test_${name} tests/test_yb_ats_al_${name}.c ${YB_SOURCES})
	target_include_directories(yb_test_${name} PRIVATE ${YB_INCLUDES})
	target_link_libraries(yb_test_${name} PRIVATE openspodes cmocka)
	target_compile_definitions(yb_test_${name} PRIVATE ${YB_OSP_MAX_OBJECTS})
	add_test(NAME yb_test_${name} COMMAND yb_test_${name})
endforeach()

add_executable(yb_test_symsec_0 tests/test_yb_ats_al_symsec_0.c ${YB_SOURCES})
target_include_directories(yb_test_symsec_0 PRIVATE ${YB_INCLUDES})
target_link_libraries(yb_test_symsec_0 PRIVATE openspodes cmocka)
target_compile_definitions(yb_test_symsec_0 PRIVATE ${YB_OSP_MAX_OBJECTS})
add_test(NAME yb_test_symsec_0 COMMAND yb_test_symsec_0)

# OpenSSL-dependent Yellow Book targets
if(OpenSSL_FOUND)
	foreach(name appl_open appl_rel appl_sec symsec_0)
		target_sources(yb_test_${name} PRIVATE tests/real_crypto.c)
		target_link_libraries(yb_test_${name} PRIVATE OpenSSL::Crypto)
		target_compile_definitions(yb_test_${name} PRIVATE OSP_HAVE_OPENSSL_GCM=1)
	endforeach()
endif()
