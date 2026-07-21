/**
 * test_yb_ats_al_appl_rel.c — Yellow Book ATS_AL: Association release
 *
 * Maps to test group APPL_REL (DLMS UA 1001-6, ATS_AL_COSEM_SYMSEC_0).
 * Tests RLRQ/RLRE release, timeout, immediate disconnect, reconnection,
 * and GET after release.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/server/dispatcher.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"
#include "mock_transport.h"
#include "mock_crypto.h"
#include "yb_helpers.h"

/* ── P1: Normal RLRQ→RLRE release ─────────────────────────────────────── */

static void test_appl_rel_normal(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 42);

	/* Normal release */
	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ── P7: GET after release fails ───────────────────────────────────────── */

static void test_appl_rel_then_get_fails(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Release */
	assert_int_equal(osp_client_release(&client), OSP_OK);

	/* GET after release should fail */
	osp_value_t result;
	osp_err_t r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result);
	assert_int_not_equal(r, OSP_OK);
}

/* ── P8: DISC without RLRQ/RLRE ───────────────────────────────────────── */

static void test_appl_rel_immediate_disc(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Immediate disconnect without RLRQ */
	osp_err_t r = osp_client_disconnect(&client);
	/* Disconnect should succeed (transport close) */
	assert_int_equal(r, OSP_OK);
}

/* ── P9: Connect→release→reconnect cycle ──────────────────────────────── */

static void test_appl_rel_reconnect_cycle(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	/* Connect */
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Verify connection works */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result), OSP_OK);

	/* Release */
	assert_int_equal(osp_client_release(&client), OSP_OK);

	/* Full re-init for re-association */
	mock_transport_pair_init(&pair);
	yb_setup_server(&server, &pair, 42);
	yb_setup_loopback(&pair, &server);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 42);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_appl_rel_normal),
		cmocka_unit_test(test_appl_rel_then_get_fails),
		cmocka_unit_test(test_appl_rel_immediate_disc),
		cmocka_unit_test(test_appl_rel_reconnect_cycle),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
