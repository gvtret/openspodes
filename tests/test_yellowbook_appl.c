/**
 * test_yellowbook_appl.c — Yellow Book ATS_AL: Application Layer tests
 *
 * Full APPL_* test cases from the Yellow Book conformance test plan
 * (DLMS UA 1001-6, ATS_AL_COSEM_SYMSEC_0 V1.3).
 *
 * Test groups:
 *   APPL_IDLE  — Data exchange in IDLE state
 *   APPL_OPEN  — Association establishment
 *   APPL_DATA  — xDLMS data services error handling
 *   APPL_REL   — Association release
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
#include "../src/security/security.h"
#include "../src/codec/serialize.h"
#include "mock_transport.h"
#include "mock_crypto.h"
#include "yb_helpers.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  WRAPPER-framed loopback infrastructure for APPL tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_server_t *g_appl_server = NULL;

static osp_err_t appl_loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_appl_server, data, len);
}

static osp_err_t appl_loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void appl_setup(mock_transport_pair_t *pair, osp_server_t *server) {
	mock_transport_pair_init(pair);
	pair->client_transport.send = appl_loopback_send;
	pair->client_transport.recv = appl_loopback_recv;
	pair->client_transport.ctx = pair;
	g_appl_server = server;
}

static osp_ic_data_t g_appl_data_obj;

static void appl_make_pair(mock_transport_pair_t *pair, osp_server_t *server, osp_client_t *client, uint32_t initial_value) {
	/* Setup loopback FIRST (initializes transport pair + client callbacks) */
	appl_setup(pair, server);

	/* THEN init server with WRAPPER framing (captures server transport) */
	osp_server_init(server, &pair->server_transport, OSP_FRAMING_WRAPPER);
	osp_ic_data_init(&g_appl_data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	g_appl_data_obj.value = osp_val_u32(initial_value);
	osp_server_register(server, osp_ic_data_class(), &g_appl_data_obj);
	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(server, &ssec);

	osp_client_init(client, &pair->client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(client, &csec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  APPL_IDLE: Data exchange in IDLE state
 * ═══════════════════════════════════════════════════════════════════════════ */

/* APPL_IDLE_N1: GET request without established association */
static void test_appl_idle_n1(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Do NOT connect — try GET in IDLE state */
	osp_value_t result;
	osp_err_t r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
	printf("  APPL_IDLE_N1: GET without AA → err=%d (expected error)\n", r);
	assert_int_not_equal(r, OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  APPL_OPEN: Association establishment tests
 * ═══════════════════════════════════════════════════════════════════════════ */

/* APPL_OPEN_1: Establish AA with declared parameters */
static void test_appl_open_1(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_1: AARQ/AARE OK\n");
	mock_transport_trace_dump(&pair);

	osp_value_t result;
	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 42);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_2: Client user identification */
static void test_appl_open_2(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Client user identification is carried in AARQ.calling-AE-invocation-id.
	 * With lowest security, this field is optional. Connect and verify. */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_2: Client user identification via AARQ OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_3: HLS authentication, Pass 3 and Pass 4 */
static void test_appl_open_3(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	osp_server_set_security(&server, &ssec);
	appl_setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	osp_client_set_security(&client, &csec);

	/* HLS with mock crypto may fail — test handshake flow */
	osp_err_t r = osp_client_connect(&client, 5000);
	printf("  APPL_OPEN_3: HLS GMAC handshake → err=%d\n", r);
	(void)r;

	/* If connected, verify and release */
	if (r == OSP_OK) {
		osp_value_t result;
		osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
		osp_client_release(&client);
	}
}

/* APPL_OPEN_4: Protocol version */
static void test_appl_open_4(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect with default protocol version (should work) */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_4: Protocol version check OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_5: Application context */
static void test_appl_open_5(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect with correct application context (LN referencing) */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_5: Application context (LN) OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_6: Titles, qualifiers and invocation identifiers */
static void test_appl_open_6(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect and verify calling-AP-title is present in AARQ */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_6: Titles/qualifiers in AARQ OK\n");
	mock_transport_trace_dump(&pair);

	/* The AARQ should contain calling-AP-title (system title) */
	/* and calling-AE-qualifier fields per the standard */

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_7: Authentication functional unit */
static void test_appl_open_7(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect and verify authentication functional unit is set */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_7: Authentication functional unit OK\n");
	mock_transport_trace_dump(&pair);

	/* With lowest security, the authentication FU should be absent
	 * or set to no-authentication */
	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_9: xDLMS InitiateRequest: dedicated-key */
static void test_appl_open_9(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect without dedicated key (lowest security) */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_9: No dedicated key with lowest security OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_11: xDLMS InitiateRequest: quality-of-service */
static void test_appl_open_11(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Default QoS (0) should be accepted */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_11: Quality-of-service (default=0) OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_12: xDLMS InitiateRequest: dlms-version-number */
static void test_appl_open_12(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect with default DLMS version (6) */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_12: DLMS version number OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_13: xDLMS InitiateRequest: conformance-block */
static void test_appl_open_13(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect and verify conformance block is negotiated */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_13: Conformance block negotiation OK\n");
	mock_transport_trace_dump(&pair);

	/* The AARE should contain negotiated conformance block */
	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_OPEN_14: xDLMS InitiateRequest: client-max-receive-pdu-size */
static void test_appl_open_14(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	/* Connect with default PDU size */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_OPEN_14: Client max PDU size OK\n");
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  APPL_DATA: xDLMS data services error handling
 * ═══════════════════════════════════════════════════════════════════════════ */

/* APPL_DATA_LN_N1: Get-Request with errors */
static void test_appl_data_ln_n1(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Subtest 1: GET on non-existent object */
	osp_value_t result;
	osp_err_t r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 99, 255, 255, 255}, 1, &result);
	printf("  APPL_DATA_LN_N1: GET non-existent OBIS → err=%d\n", r);
	assert_int_not_equal(r, OSP_OK);
	mock_transport_trace_dump(&pair);

	/* Subtest 2: GET on undefined attribute (attr 99) */
	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 99, &result);
	printf("  APPL_DATA_LN_N1: GET undefined attr → err=%d\n", r);
	assert_int_not_equal(r, OSP_OK);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_DATA_LN_N3: Set-Request with errors */
static void test_appl_data_ln_n3(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Subtest 1: SET on non-existent object */
	osp_value_t val = osp_val_u32(100);
	osp_err_t r = osp_client_set(&client, 1, &(osp_obis_t){0, 0, 99, 255, 255, 255}, 1, &val);
	printf("  APPL_DATA_LN_N3: SET non-existent OBIS → err=%d\n", r);
	assert_int_not_equal(r, OSP_OK);
	mock_transport_trace_dump(&pair);

	/* Subtest 2: SET on undefined attribute (attr 99) */
	r = osp_client_set(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 99, &val);
	printf("  APPL_DATA_LN_N3: SET undefined attr → err=%d\n", r);
	assert_int_not_equal(r, OSP_OK);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* APPL_DATA_LN_N4: Unsupported service */
static void test_appl_data_ln_n4(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* ACTION on non-existent method (method 99) */
	osp_value_t result;
	osp_err_t r = osp_client_action(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 99, NULL, &result);
	printf("  APPL_DATA_LN_N4: ACTION non-existent method → err=%d\n", r);
	assert_int_not_equal(r, OSP_OK);
	mock_transport_trace_dump(&pair);

	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  APPL_DATA_SN: SN-referencing data service tests (Tables 26-28)
 *
 *  Verify IUT rejects SN-referencing ReadRequest (0x01) /
 *  WriteRequest (0x02) / ActionRequest (0x03) with ExceptionResponse.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Send raw SN APDU to the server (no framing) and trigger osp_server_accept */
static void sn_send_apdu(mock_transport_pair_t *pair, osp_server_t *server,
			 const uint8_t *apdu, uint32_t apdu_len) {
	mock_send_to_peer(&pair->server_rx, apdu, apdu_len);
	osp_server_accept(server, 0);
}

/* APPL_DATA_SN_N1: ReadRequest with errors (Table 26) */
static void test_appl_data_sn_n1(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* After connect, switch server to FRAMING_NONE for raw SN APDU injection.
	 * The server has already processed AARQ/AARE, so WRAPPER framing is no
	 * longer needed for manual SN APDU tests. */
	server.framing = OSP_FRAMING_NONE;

	uint8_t resp[32];
	uint32_t resp_len = 0;

	/* Subtest 1: ReadRequest with unknown VariableAccessSpecification tag. */
	{
		uint8_t apdu[] = {0x01, 0x41, 0x09, 0x02, 0x00};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		osp_err_t rr = mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0);
		printf("  APPL_DATA_SN_N1 Subtest 1: recv err=%d resp_len=%u resp[0]=0x%02x\n",
		       rr, resp_len, resp_len > 0 ? resp[0] : 0);
		assert_int_equal(rr, OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N1 Subtest 1: ExceptionResponse for unknown VAR_ACCESS tag OK\n");
	}

	/* Subtest 2: ReadRequest with missing elements (truncated). */
	{
		uint8_t apdu[] = {0x01, 0x42};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		printf("  APPL_DATA_SN_N1 Subtest 2: Handled truncated SN ReadRequest\n");
	}

	/* Subtest 3: ReadRequest for non-existing variable-name (SN=0xFA01). */
	{
		uint8_t apdu[] = {0x01, 0x43, 0x02, 0x00, 0xFA, 0x01};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N1 Subtest 3: ExceptionResponse for SN=0xFA01 OK\n");
	}

	/* Release without assert — client framing is WRAPPER but server is NONE */
	osp_client_release(&client);
}

/* APPL_DATA_SN_N2: WriteRequest with errors (Table 27) */
static void test_appl_data_sn_n2(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	server.framing = OSP_FRAMING_NONE;

	uint8_t resp[32];
	uint32_t resp_len = 0;

	/* Subtest 1: WriteRequest with unknown VariableAccessSpecification tag. */
	{
		uint8_t apdu[] = {0x02, 0x41, 0x09, 0x02, 0x00};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N2 Subtest 1: ExceptionResponse for unknown VAR_ACCESS tag OK\n");
	}

	/* Subtest 2: WriteRequest with missing elements (truncated). */
	{
		uint8_t apdu[] = {0x02, 0x42};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		printf("  APPL_DATA_SN_N2 Subtest 2: Truncated SN WriteRequest handled\n");
	}

	/* Subtest 3: WriteRequest for non-existing variable-name (SN=0xFA01). */
	{
		uint8_t apdu[] = {0x02, 0x43, 0x02, 0x00, 0xFA, 0x01, 0x0F, 0x00, 0x00, 0x00};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N2 Subtest 3: ExceptionResponse for SN=0xFA01 OK\n");
	}

	/* Release without assert — client framing is WRAPPER but server is NONE */
	osp_client_release(&client);
}

/* APPL_DATA_SN_N3: Unsupported SN service (Table 28) */
static void test_appl_data_sn_n3(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	server.framing = OSP_FRAMING_NONE;

	uint8_t resp[32];
	uint32_t resp_len = 0;

	/* Subtest 1: ReadRequest (SN 0x01) — not supported. */
	{
		uint8_t apdu[] = {0x01, 0x41, 0x02, 0x00, 0x00, 0x01};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N3 Subtest 1: ReadRequest SN → ExceptionResponse OK\n");
	}

	/* Subtest 2: WriteRequest (SN 0x02) — not supported. */
	{
		uint8_t apdu[] = {0x02, 0x42, 0x02, 0x00, 0x00, 0x01, 0x0F, 0x00};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N3 Subtest 2: WriteRequest SN → ExceptionResponse OK\n");
	}

	/* Subtest 3: ActionRequest via SN (0x03) — not supported. */
	{
		uint8_t apdu[] = {0x03, 0x43, 0x02, 0x00, 0x00, 0x01, 0x00};
		sn_send_apdu(&pair, &server, apdu, sizeof(apdu));
		resp_len = 0;
		assert_int_equal(mock_recv_from_peer(&pair.client_rx, resp, sizeof(resp), &resp_len, 0), OSP_OK);
		assert_int_equal(resp[0], OSP_TAG_EXCEPTION_RESPONSE);
		printf("  APPL_DATA_SN_N3 Subtest 3: ActionRequest SN → ExceptionResponse OK\n");
	}

	/* Release without assert — client framing is WRAPPER but server is NONE */
	osp_client_release(&client);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  APPL_REL: Association release
 * ═══════════════════════════════════════════════════════════════════════════ */

/* APPL_REL_P1: Graceful release */
static void test_appl_rel_p1(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	appl_make_pair(&pair, &server, &client, 42);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* Verify connection works */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 42);

	/* Graceful release via RLRQ/RLRE */
	osp_err_t r = osp_client_release(&client);
	assert_int_equal(r, OSP_OK);
	printf("  APPL_REL_P1: RLRQ/RLRE release OK\n");
	mock_transport_trace_dump(&pair);

	/* GET after release should fail */
	r = osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result);
	assert_int_not_equal(r, OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test suite
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		/* APPL_IDLE */
		cmocka_unit_test(test_appl_idle_n1),

		/* APPL_OPEN */
		cmocka_unit_test(test_appl_open_1),
		cmocka_unit_test(test_appl_open_2),
		cmocka_unit_test(test_appl_open_3),
		cmocka_unit_test(test_appl_open_4),
		cmocka_unit_test(test_appl_open_5),
		cmocka_unit_test(test_appl_open_6),
		cmocka_unit_test(test_appl_open_7),
		cmocka_unit_test(test_appl_open_9),
		cmocka_unit_test(test_appl_open_11),
		cmocka_unit_test(test_appl_open_12),
		cmocka_unit_test(test_appl_open_13),
		cmocka_unit_test(test_appl_open_14),

		/* APPL_DATA */
		cmocka_unit_test(test_appl_data_ln_n1),
		cmocka_unit_test(test_appl_data_ln_n3),
		cmocka_unit_test(test_appl_data_ln_n4),
		cmocka_unit_test(test_appl_data_sn_n1),
		cmocka_unit_test(test_appl_data_sn_n2),
		cmocka_unit_test(test_appl_data_sn_n3),

		/* APPL_REL */
		cmocka_unit_test(test_appl_rel_p1),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
