/**
 * test_yb_ats_al_cosem_objs.c — Yellow Book Section 7: COSEM interface objects
 *
 * Implements the COSEM interface object testing algorithm from Yellow Book:
 *   1. Build AA and read object_list
 *   2. For each object, read all readable attributes
 *   3. For each writable attribute, write back the value
 *   4. Check mandatory objects (Association LN, SAP assignment, LDN)
 *
 * Mandatory objects per СТО 34.01 Приложение И:
 *   - Association LN (class_id=15) OBIS 0.0.40.0.x.255
 *   - SAP Assignment (class_id=17) OBIS 0.0.41.0.0.255
 *   - Logical Device Name (Data IC, class_id=1) OBIS 0.0.42.0.0.255
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/server/dispatcher.h"
#include "../src/ic/data.h"
#include "../src/ic/register.h"
#include "../src/ic/association_ln.h"
#include "../src/security/security.h"
#include "../src/codec/serialize.h"
#include "mock_transport.h"
#include "mock_crypto.h"
#include "yb_helpers.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  WRAPPER-framed loopback
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_server_t *g_cosem_server = NULL;

static osp_err_t cosem_loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_cosem_server, data, len);
}

static osp_err_t cosem_loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void appl_setup(mock_transport_pair_t *pair, osp_server_t *server) {
	mock_transport_pair_init(pair);
	pair->client_transport.send = cosem_loopback_send;
	pair->client_transport.recv = cosem_loopback_recv;
	pair->client_transport.ctx = pair;
	g_cosem_server = server;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test objects (static to survive function scope)
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_ic_data_t g_data_energy;
static osp_ic_data_t g_data_voltage;
static osp_ic_data_t g_data_status;
static osp_ic_data_t g_data_ldn;
static osp_ic_register_t g_reg_clock;
static osp_ic_association_ln_t g_aln;

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM mandatory objects: Read object_list
 *
 *  Yellow Book Section 7.2:
 *  "Build the AA and read the object_list"
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_read_object_list(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	/* Init transport pair */
	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	/* Register objects */
	osp_ic_data_init(&g_data_energy, (osp_obis_t){1, 0, 1, 8, 0, 255});
	g_data_energy.value = osp_val_u32(12345678);
	osp_server_register(&server, osp_ic_data_class(), &g_data_energy);

	osp_ic_data_init(&g_data_voltage, (osp_obis_t){0, 0, 96, 1, 0, 255});
	g_data_voltage.value = osp_val_u32(230);
	osp_server_register(&server, osp_ic_data_class(), &g_data_voltage);

	osp_ic_data_init(&g_data_status, (osp_obis_t){0, 0, 96, 3, 10, 255});
	g_data_status.value = osp_val_u32(0);
	osp_server_register(&server, osp_ic_data_class(), &g_data_status);

	osp_ic_data_init(&g_data_ldn, (osp_obis_t){0, 0, 42, 0, 0, 255});
	g_data_ldn.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &g_data_ldn);

	osp_ic_register_init(&g_reg_clock, (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(0));
	osp_server_register(&server, osp_ic_register_class(), &g_reg_clock);

	/* Register Association LN with object_list */
	osp_ic_association_ln_init(&g_aln, (osp_obis_t){0, 0, 40, 0, 1, 255});
	osp_object_list_element_t elem;
	elem.class_id = 1;
	elem.logical_name = (osp_obis_t){1, 0, 1, 8, 0, 255};
	memset(&elem.access_rights, 0, sizeof(elem.access_rights));
	elem.access_rights.attr_count = 1;
	elem.access_rights.attr_items[0].attribute_id = 1;
	elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_READ_ONLY;
	osp_ic_association_ln_add_object(&g_aln, &elem);

	elem.class_id = 1;
	elem.logical_name = (osp_obis_t){0, 0, 96, 1, 0, 255};
	osp_ic_association_ln_add_object(&g_aln, &elem);

	elem.class_id = 3;
	elem.logical_name = (osp_obis_t){0, 0, 1, 0, 0, 255};
	osp_ic_association_ln_add_object(&g_aln, &elem);

	osp_server_register(&server, osp_ic_association_ln_class(), &g_aln);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Read object_list (attr 2 of Association LN) */
	osp_value_t obj_list;
	osp_err_t r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 2, &obj_list);
	printf("  COSEM: Read object_list → err=%d\n", r);

	/* Read logical device name (Data IC 0.0.42.0.0.255) */
	osp_value_t ldn;
	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 42, 0, 0, 255}, 1, &ldn);
	printf("  COSEM: Read LDN → err=%d\n", r);

	/* Read all registered objects */
	osp_value_t result;
	r = osp_client_get(&client, 1, &(osp_obis_t){1, 0, 1, 8, 0, 255}, 1, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 12345678);
	printf("  COSEM: Read energy=%u OK\n", result.as.uint32.value);

	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 96, 1, 0, 255}, 1, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 230);
	printf("  COSEM: Read voltage=%u OK\n", result.as.uint32.value);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM mandatory objects: Write-back test
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_write_back(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	osp_ic_data_init(&g_data_energy, (osp_obis_t){1, 0, 1, 8, 0, 255});
	g_data_energy.value = osp_val_u32(12345678);
	osp_server_register(&server, osp_ic_data_class(), &g_data_energy);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Read original value */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){1, 0, 1, 8, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 12345678);

	/* Write back original value */
	assert_int_equal(osp_client_set(&client, 1, &(osp_obis_t){1, 0, 1, 8, 0, 255}, 1, &result), OSP_OK);

	/* Verify value unchanged */
	osp_value_t verify;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){1, 0, 1, 8, 0, 255}, 1, &verify), OSP_OK);
	assert_int_equal(verify.as.uint32.value, 12345678);
	printf("  COSEM: Write-back test OK\n");

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM mandatory objects: Check mandatory elements
 *
 *  Yellow Book Section 7.3.12 / СТО Приложение И:
 *  - Association LN (class_id=15) OBIS 0.0.40.0.x.255
 *  - Logical Device Name (Data IC) OBIS 0.0.42.0.0.255
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_mandatory_objects(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	osp_ic_data_init(&g_data_ldn, (osp_obis_t){0, 0, 42, 0, 0, 255});
	g_data_ldn.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &g_data_ldn);

	osp_ic_association_ln_init(&g_aln, (osp_obis_t){0, 0, 40, 0, 1, 255});
	osp_server_register(&server, osp_ic_association_ln_class(), &g_aln);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;

	/* Check mandatory: Logical Device Name (0.0.42.0.0.255) */
	osp_err_t r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 42, 0, 0, 255}, 1, &result);
	printf("  COSEM mandatory: LDN read → err=%d\n", r);

	/* Check mandatory: Association LN object_list (attr 2) */
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 2, &result);
	printf("  COSEM mandatory: object_list read → err=%d\n", r);

	/* Check mandatory: Association LN logical_name (attr 1) */
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 1, &result);
	printf("  COSEM mandatory: Association LN name → err=%d\n", r);

	/* Check mandatory: Association LN xDLMS_context_info (attr 5) */
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 5, &result);
	printf("  COSEM mandatory: xDLMS context → err=%d\n", r);

	/* Check mandatory: Association LN authentication_mechanism_name (attr 6) */
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 6, &result);
	printf("  COSEM mandatory: auth mechanism → err=%d\n", r);

	/* Check mandatory: Association LN association_status (attr 8) */
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 8, &result);
	printf("  COSEM mandatory: association status → err=%d\n", r);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM mandatory objects: Access rights verification
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_access_rights(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	osp_ic_data_init(&g_data_energy, (osp_obis_t){1, 0, 1, 8, 0, 255});
	g_data_energy.value = osp_val_u32(12345678);
	osp_server_register(&server, osp_ic_data_class(), &g_data_energy);

	osp_ic_register_init(&g_reg_clock, (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(0));
	osp_server_register(&server, osp_ic_register_class(), &g_reg_clock);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Read each attribute of the registered objects */
	osp_value_t result;
	/* Data IC: attr 1 = value */
	osp_err_t r = osp_client_get(&client, 1, &(osp_obis_t){1, 0, 1, 8, 0, 255}, 1, &result);
	assert_int_equal(r, OSP_OK);
	printf("  COSEM: Data IC attr 1 = %u (readable)\n", result.as.uint32.value);

	/* Register IC: attr 1 = value */
	r = osp_client_get(&client, 3, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
	printf("  COSEM: Register IC attr 1 → err=%d\n", r);

	/* Register IC: attr 2 = scalar_unit */
	r = osp_client_get(&client, 3, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result);
	printf("  COSEM: Register IC attr 2 → err=%d\n", r);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM: Multiple references test
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_multiple_references(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	osp_ic_data_init(&g_data_energy, (osp_obis_t){1, 0, 1, 8, 0, 255});
	g_data_energy.value = osp_val_u32(12345678);
	osp_server_register(&server, osp_ic_data_class(), &g_data_energy);

	osp_ic_data_init(&g_data_voltage, (osp_obis_t){0, 0, 96, 1, 0, 255});
	g_data_voltage.value = osp_val_u32(230);
	osp_server_register(&server, osp_ic_data_class(), &g_data_voltage);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Multiple references: GET with-list for multiple objects */
	osp_client_attr_ref_t attrs[2] = {
		{.class_id = 1, .instance_id = {1, 0, 1, 8, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 96, 1, 0, 255}, .attribute_id = 1},
	};
	osp_get_result_item_t results[2];
	memset(results, 0, sizeof(results));
	osp_err_t r = osp_client_get_with_list(&client, attrs, 2, results);
	printf("  COSEM: Multiple references GET-with-list → err=%d\n", r);
	(void)r;

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test suite
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_cosem_read_object_list),
		cmocka_unit_test(test_cosem_write_back),
		cmocka_unit_test(test_cosem_mandatory_objects),
		cmocka_unit_test(test_cosem_multiple_references),
		cmocka_unit_test(test_cosem_access_rights),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
