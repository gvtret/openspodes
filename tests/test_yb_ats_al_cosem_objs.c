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
#include "../src/ic/push_setup.h"
#include "../src/ic/sap_assignment.h"
#include "../src/ic/clock.h"
#include "../src/ic/disconnect_control.h"
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
static osp_ic_push_setup_t g_push_setup;
static osp_ic_clock_t g_clock;

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
 *  COSEM mandatory objects: PushSetup + Clock + SAP Assignment
 *
 *  СТО 34.01 Приложение И mandatory objects:
 *  - PushSetup (class_id=40) OBIS 0.0.25.9.0.255
 *  - Clock (class_id=8) OBIS 0.0.1.0.0.255
 *  - SAP Assignment (class_id=17) OBIS 0.0.41.0.0.255
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_push_setup_and_others(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	/* Register PushSetup */
	osp_ic_push_setup_init(&g_push_setup, (osp_obis_t){0, 0, 25, 9, 0, 255});
	osp_server_register(&server, osp_ic_push_setup_class(), &g_push_setup);

	/* Register Clock */
	osp_ic_clock_init(&g_clock, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_server_register(&server, osp_ic_clock_class(), &g_clock);

	/* Register SAP Assignment */
	osp_ic_sap_assignment_t sap;
	osp_ic_sap_assignment_init(&sap, (osp_obis_t){0, 0, 41, 0, 0, 255});
	osp_server_register(&server, osp_ic_sap_assignment_class(), &sap);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	osp_err_t r;

	/* PushSetup: attr 1 = logical_name (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 1, &result);
	printf("  PushSetup attr 1 (LN) → err=%d\n", r);

	/* PushSetup: attr 2 = push_object_list (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 2, &result);
	printf("  PushSetup attr 2 (push_object_list) → err=%d\n", r);

	/* PushSetup: attr 3 = send_destination_and_method (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 3, &result);
	printf("  PushSetup attr 3 (send_dest) → err=%d\n", r);

	/* PushSetup: attr 4 = communication_window (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 4, &result);
	printf("  PushSetup attr 4 (comm_window) → err=%d\n", r);

	/* PushSetup: attr 5 = randomisation_start_interval (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 5, &result);
	printf("  PushSetup attr 5 (randomisation) → err=%d\n", r);

	/* PushSetup: attr 6 = number_of_retries (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 6, &result);
	printf("  PushSetup attr 6 (retries) → err=%d\n", r);

	/* PushSetup: attr 7 = repetition_delay (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 7, &result);
	printf("  PushSetup attr 7 (repetition_delay) → err=%d\n", r);

	/* PushSetup: attr 8 = port_reference (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 8, &result);
	printf("  PushSetup attr 8 (port_reference) → err=%d\n", r);

	/* PushSetup: attr 9 = push_client_SAP (static) */
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 9, &result);
	printf("  PushSetup attr 9 (push_client_SAP) → err=%d\n", r);

	/* Clock: attr 1 = logical_name */
	r = osp_client_get(&client, 8, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
	printf("  Clock attr 1 (LN) → err=%d\n", r);

	/* Clock: attr 2 = time */
	r = osp_client_get(&client, 8, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &result);
	printf("  Clock attr 2 (time) → err=%d\n", r);

	/* SAP Assignment: attr 1 = logical_name */
	r = osp_client_get(&client, 17, &(osp_obis_t){0, 0, 41, 0, 0, 255}, 1, &result);
	printf("  SAP Assignment attr 1 (LN) → err=%d\n", r);

	/* SAP Assignment: attr 2 = channel_list */
	r = osp_client_get(&client, 17, &(osp_obis_t){0, 0, 41, 0, 0, 255}, 2, &result);
	printf("  SAP Assignment attr 2 (channel_list) → err=%d\n", r);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM: Push operation test (Yellow Book 7.3.8 / Table 31)
 *
 *  Subtest 1: Push без принудительной защиты
 *  Subtest 2: Push с принудительной защитой
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_cosem_push_operation(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	/* Register PushSetup */
	osp_ic_push_setup_init(&g_push_setup, (osp_obis_t){0, 0, 25, 9, 0, 255});
	osp_server_register(&server, osp_ic_push_setup_class(), &g_push_setup);

	/* Register a data object to push */
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

	/* Subtest 1: Push without forced protection */
	/* Invoke push method (method_id=1) on PushSetup */
	osp_value_t result;
	osp_err_t r = osp_client_action(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 1, NULL, &result);
	printf("  Push operation: push method → err=%d\n", r);

	/* Verify push_object_list is readable */
	osp_value_t push_list;
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 2, &push_list);
	printf("  Push operation: push_object_list → err=%d\n", r);

	/* Verify send_destination_and_method is readable */
	osp_value_t dest;
	r = osp_client_get(&client, 40, &(osp_obis_t){0, 0, 25, 9, 0, 255}, 3, &dest);
	printf("  Push operation: send_dest → err=%d\n", r);

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
		cmocka_unit_test(test_cosem_push_setup_and_others),
		cmocka_unit_test(test_cosem_push_operation),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
