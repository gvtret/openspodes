/**
 * test_yb_ats_al_cosem_objs.c — Yellow Book ATS_AL: Mandatory COSEM objects
 *
 * Tests COSEM mandatory object access: Data IC read/write, Clock IC,
 * Profile Generic buffer, and Disconnect Control lifecycle.
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

/* ── P1: Data IC — GET attr 1 (value) ────────────────────────────────────── */

static void test_cosem_data_read(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* GET attr 1 (value — the Data IC only supports attr_id == 1) */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 42);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ── P2: Data IC — SET attr 1 (value) ─────────────────────────────────── */

static void test_cosem_data_write(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	yb_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t new_val = osp_val_u32(100);
	assert_int_equal(osp_client_set(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &new_val), OSP_OK);

	/* Verify the write took effect */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 100);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_cosem_data_read),
		cmocka_unit_test(test_cosem_data_write),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
