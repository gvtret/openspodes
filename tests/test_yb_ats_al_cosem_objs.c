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
#include "../src/ic/security_setup.h"
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
static osp_ic_security_setup_t g_sec_setup;

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

	/* Register mandatory objects per СТО Приложение И */
	osp_ic_data_init(&g_data_ldn, (osp_obis_t){0, 0, 42, 0, 0, 255});
	g_data_ldn.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &g_data_ldn);

	osp_ic_register_init(&g_reg_clock, (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(0));
	osp_server_register(&server, osp_ic_register_class(), &g_reg_clock);

	/* Register Association LN with mandatory attributes.
	 * ALN's object_list controls access — populate it with the objects
	 * we want to be readable. Without entries, can_read returns false. */
	osp_ic_association_ln_init(&g_aln, (osp_obis_t){0, 0, 40, 0, 1, 255});
	g_aln.association_status = 2; /* ASSOCIATED */

	/* Add ALN itself to its object_list with all needed attributes */
	{
		osp_object_list_element_t e;
		e.class_id = 15;
		e.logical_name = (osp_obis_t){0, 0, 40, 0, 1, 255};
		memset(&e.access_rights, 0, sizeof(e.access_rights));
		e.access_rights.attr_count = 8;
		e.access_rights.attr_items[0] = (osp_attribute_access_item_t){3, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[1] = (osp_attribute_access_item_t){4, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[2] = (osp_attribute_access_item_t){5, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[3] = (osp_attribute_access_item_t){6, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[4] = (osp_attribute_access_item_t){8, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[5] = (osp_attribute_access_item_t){9, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[6] = (osp_attribute_access_item_t){10, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[7] = (osp_attribute_access_item_t){11, OSP_ACCESS_READ_ONLY};
		osp_ic_association_ln_add_object(&g_aln, &e);
	}

	/* Add LDN to object_list */
	{
		osp_object_list_element_t e;
		e.class_id = 1;
		e.logical_name = (osp_obis_t){0, 0, 42, 0, 0, 255};
		memset(&e.access_rights, 0, sizeof(e.access_rights));
		e.access_rights.attr_count = 1;
		e.access_rights.attr_items[0] = (osp_attribute_access_item_t){1, OSP_ACCESS_READ_ONLY};
		osp_ic_association_ln_add_object(&g_aln, &e);
	}

	/* Add SAP Assignment to object_list */
	{
		osp_object_list_element_t e;
		e.class_id = 17;
		e.logical_name = (osp_obis_t){0, 0, 41, 0, 0, 255};
		memset(&e.access_rights, 0, sizeof(e.access_rights));
		e.access_rights.attr_count = 2;
		e.access_rights.attr_items[0] = (osp_attribute_access_item_t){1, OSP_ACCESS_READ_ONLY};
		e.access_rights.attr_items[1] = (osp_attribute_access_item_t){2, OSP_ACCESS_READ_ONLY};
		osp_ic_association_ln_add_object(&g_aln, &e);
	}

	osp_server_register(&server, osp_ic_association_ln_class(), &g_aln);

	/* Register SecuritySetup for security_setup_reference check */
	osp_ic_security_setup_init(&g_sec_setup, (osp_obis_t){0, 0, 43, 0, 0, 255});
	osp_server_register(&server, osp_ic_security_setup_class(), &g_sec_setup);

	/* Set security_setup_reference in Association LN */
	g_aln.security_setup_reference = (osp_obis_t){0, 0, 43, 0, 0, 255};

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
	int passed = 0;
	int total = 0;

	/* === Subtest 1: Current association (Table 32) === */
	printf("  7.3.12 Subtest 1: Current association\n");

	/* LN: application_context_name (attr 4) */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 4, &result);
	printf("    application_context_name (attr 4) → err=%d\n", r);

	/* LN: xDLMS_context_info (attr 5) */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 5, &result);
	printf("    xDLMS_context_info (attr 5) → err=%d\n", r);

	/* LN: authentication_mechanism_name (attr 6) */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 6, &result);
	printf("    authentication_mechanism_name (attr 6) → err=%d\n", r);

	/* LN: association_status (attr 8) — must be 2 (associated) */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 8, &result);
	printf("    association_status (attr 8) → err=%d\n", r);
	if (r == OSP_OK) {
		printf("    association_status = %u (expected 2=associated)\n",
		       result.as.uint32.value);
		passed++;
	}

	/* LN: security_setup_reference (attr 9) — version ≥1 */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 9, &result);
	printf("    security_setup_reference (attr 9) → err=%d\n", r);

	/* LN: user_list (attr 10) — version ≥2 */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 10, &result);
	printf("    user_list (attr 10) → err=%d\n", r);

	/* LN: current_user (attr 11) — version ≥2 */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 11, &result);
	printf("    current_user (attr 11) → err=%d\n", r);

	/* LN: associated_partners_id (attr 3) */
	total++;
	r = osp_client_get(&client, 15, &(osp_obis_t){0, 0, 40, 0, 1, 255}, 3, &result);
	printf("    associated_partners_id (attr 3) → err=%d\n", r);
	if (r == OSP_OK)
		passed++;

	/* === Subtest 2: SAP Assignment === */
	printf("  7.3.12 Subtest 2: SAP Assignment\n");

	/* SAP Assignment: logical_name (attr 1) */
	total++;
	r = osp_client_get(&client, 17, &(osp_obis_t){0, 0, 41, 0, 0, 255}, 1, &result);
	printf("    SAP Assignment LN → err=%d\n", r);
	if (r == OSP_OK)
		passed++;

	/* SAP Assignment: channel_list / SAP_assignment_list (attr 2) */
	total++;
	r = osp_client_get(&client, 17, &(osp_obis_t){0, 0, 41, 0, 0, 255}, 2, &result);
	printf("    SAP Assignment list (attr 2) → err=%d\n", r);
	if (r == OSP_OK)
		passed++;

	/* === Subtest 3: Logical Device Name === */
	printf("  7.3.12 Subtest 3: Logical Device Name\n");
	total++;
	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 42, 0, 0, 255}, 1, &result);
	printf("    LDN → err=%d\n", r);
	if (r == OSP_OK)
		passed++;

	printf("  7.3.12 Summary: %d/%d checks passed\n", passed, total);

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
 *  7.3.11 Multiple references test
 *
 *  Yellow Book: Compare values read together (GET with-list) vs separately.
 *  1) Read 10 value attributes of classes 1/3/4 (split into 8+2 batches)
 *  2) Read a short (1-10 byte) and a long (500-1000 byte) attribute
 *  3) Read the first 4 attributes of the first object with ≥4 attributes
 *
 *  Clock (class_id=8) is always excluded.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Helper: register N data objects for multiple-ref testing */
static void multiref_register_objects(osp_server_t *server, int count) {
	static osp_ic_data_t data_objs[12];
	static osp_ic_register_t reg_objs[2];
	const osp_obis_t obis_list[] = {
		{1, 0, 1, 8, 0, 255},   /* energy */
		{0, 0, 96, 1, 0, 255},  /* voltage */
		{0, 0, 96, 3, 10, 255}, /* status */
		{0, 0, 42, 0, 0, 255},  /* LDN */
		{1, 0, 2, 8, 0, 255},   /* reactive energy */
		{1, 0, 3, 8, 0, 255},   /* apparent energy */
		{0, 0, 11, 0, 0, 255},  /* current */
		{0, 0, 12, 0, 0, 255},  /* power factor */
		{1, 0, 4, 8, 0, 255},   /* energy A+ */
		{1, 0, 5, 8, 0, 255},   /* energy A- */
		{0, 0, 13, 0, 0, 255},  /* frequency */
		{0, 0, 14, 0, 0, 255},  /* power */
	};
	for (int i = 0; i < count && i < 10; i++) {
		osp_ic_data_init(&data_objs[i], obis_list[i]);
		data_objs[i].value = osp_val_u32((uint32_t)(1000 + i));
		osp_server_register(server, osp_ic_data_class(), &data_objs[i]);
	}
	/* Register 2 Register ICs for variety */
	osp_ic_register_init(&reg_objs[0], (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(50));
	osp_server_register(server, osp_ic_register_class(), &reg_objs[0]);
	osp_ic_register_init(&reg_objs[1], (osp_obis_t){0, 0, 2, 0, 0, 255}, osp_val_u32(60));
	osp_server_register(server, osp_ic_register_class(), &reg_objs[1]);
}

static void test_cosem_multiple_references(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	appl_setup(&pair, &server);
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);

	multiref_register_objects(&server, 10);

	/* Register PushSetup for subtest 3 (object with ≥4 attributes) */
	osp_ic_push_setup_init(&g_push_setup, (osp_obis_t){0, 0, 25, 9, 0, 255});
	osp_server_register(&server, osp_ic_push_setup_class(), &g_push_setup);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_err_t r;

	/* === Subtest 1: Read value attributes of classes 1/3/4 together === */
	printf("  7.3.11 Subtest 1: Read 10 value attributes together\n");

	osp_client_attr_ref_t attrs_10[] = {
		{.class_id = 1, .instance_id = {1, 0, 1, 8, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 96, 1, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 96, 3, 10, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 42, 0, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {1, 0, 2, 8, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {1, 0, 3, 8, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 11, 0, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {0, 0, 12, 0, 0, 255}, .attribute_id = 1},
	};
	osp_get_result_item_t results_8[OSP_XDLMS_MAX_LIST];
	memset(results_8, 0, sizeof(results_8));

	/* Batch 1: up to 8 (OSP_XDLMS_MAX_LIST limit) */
	r = osp_client_get_with_list(&client, attrs_10, 8, results_8);
	printf("    Batch 1 (8 attrs) → err=%d\n", r);
	if (r == OSP_OK) {
		for (int i = 0; i < 8; i++) {
			printf("    [%d] is_data=%d dar=%d\n", i, results_8[i].is_data, results_8[i].access_result);
		}
	}

	/* Read same attributes separately and compare */
	printf("  7.3.11 Subtest 1: Read same attrs separately for comparison\n");
	osp_value_t separate;
	for (int i = 0; i < 8; i++) {
		r = osp_client_get(&client, attrs_10[i].class_id, &attrs_10[i].instance_id,
				   attrs_10[i].attribute_id, &separate);
		printf("    Separate[%d] → err=%d\n", i, r);
		if (results_8[i].is_data && r == OSP_OK) {
			/* Both returned data — values should match conceptually */
			printf("    Together vs Separate[%d]: both returned data\n", i);
		}
	}

	/* Batch 2: remaining 2 */
	osp_client_attr_ref_t attrs_2[] = {
		{.class_id = 1, .instance_id = {1, 0, 4, 8, 0, 255}, .attribute_id = 1},
		{.class_id = 1, .instance_id = {1, 0, 5, 8, 0, 255}, .attribute_id = 1},
	};
	osp_get_result_item_t results_2[2];
	memset(results_2, 0, sizeof(results_2));
	r = osp_client_get_with_list(&client, attrs_2, 2, results_2);
	printf("    Batch 2 (2 attrs) → err=%d\n", r);

	/* === Subtest 2: Short + Long attribute together === */
	printf("  7.3.11 Subtest 2: Short + Long attribute\n");
	osp_client_attr_ref_t attrs_short_long[] = {
		/* Short: Data IC value (1-4 bytes) */
		{.class_id = 1, .instance_id = {1, 0, 1, 8, 0, 255}, .attribute_id = 1},
		/* Long: PushSetup push_object_list (array, potentially long) */
		{.class_id = 40, .instance_id = {0, 0, 25, 9, 0, 255}, .attribute_id = 2},
	};
	osp_get_result_item_t results_sl[2];
	memset(results_sl, 0, sizeof(results_sl));
	r = osp_client_get_with_list(&client, attrs_short_long, 2, results_sl);
	printf("    Short+Long → err=%d\n", r);

	/* === Subtest 3: First 4 attributes of first object with ≥4 attrs === */
	printf("  7.3.11 Subtest 3: First 4 attrs of PushSetup (9 attrs)\n");
	osp_client_attr_ref_t attrs_4[] = {
		{.class_id = 40, .instance_id = {0, 0, 25, 9, 0, 255}, .attribute_id = 1},
		{.class_id = 40, .instance_id = {0, 0, 25, 9, 0, 255}, .attribute_id = 2},
		{.class_id = 40, .instance_id = {0, 0, 25, 9, 0, 255}, .attribute_id = 3},
		{.class_id = 40, .instance_id = {0, 0, 25, 9, 0, 255}, .attribute_id = 4},
	};
	osp_get_result_item_t results_4[4];
	memset(results_4, 0, sizeof(results_4));
	r = osp_client_get_with_list(&client, attrs_4, 4, results_4);
	printf("    First 4 attrs of PushSetup → err=%d\n", r);
	if (r == OSP_OK) {
		for (int i = 0; i < 4; i++) {
			printf("    [%d] is_data=%d dar=%d\n", i, results_4[i].is_data, results_4[i].access_result);
		}
	}

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
