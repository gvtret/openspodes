/**
 * test_integration.c — End-to-end integration tests
 *
 * Full client↔server flow through loopback transport:
 * AARQ→AARE→HLS handshake→GET/SET/RELEASE.
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
#include "../src/ic/disconnect_control.h"
#include "../src/ic/image_transfer.h"
#include "../src/ic/compact_data.h"
#include "../src/ic/push_setup.h"
#include "../src/service/notification.h"
#include "../src/security/security.h"
#include "../src/security/gost_crypto.h"
#include "../src/codec/serialize.h"
#include "mock_transport.h"
#include "mock_crypto.h"

/* Global server reference for loopback transport */
static osp_server_t *g_server = NULL;

/* ── Loopback transport: auto-processes server on client send ────────────── */

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_server, data, len);
}

static osp_err_t loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void setup(mock_transport_pair_t *pair, osp_server_t *server) {
	mock_transport_pair_init(pair);
	pair->client_transport.send = loopback_send;
	pair->client_transport.recv = loopback_recv;
	pair->client_transport.ctx = pair;
	g_server = server;
}

/* Helper: create a low-security client+server pair */
static void make_pair(mock_transport_pair_t *pair, osp_server_t *server, osp_client_t *client) {
	setup(pair, server);
	osp_client_init(client, &pair->client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(client, &csec);
	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(server, &ssec);
}

/* ── Test: full AARQ→HLS→GET ────────────────────────────────────────────── */

static void test_aarq_hls_get(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_value_t init_val = osp_val_u32(42);
	data_obj.value = init_val;
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	/* Connect */
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* GET value (should be 42) */
	osp_value_t result;
	osp_obis_t dobis = {0, 0, 1, 0, 0, 255};
	assert_int_equal(osp_client_get(&client, 1, &dobis, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 42);

	/* SET new value */
	osp_value_t newval = osp_val_u32(100);
	assert_int_equal(osp_client_set(&client, 1, &dobis, 1, &newval), OSP_OK);
	osp_value_t get_val;
	get_val = data_obj.value;
	assert_int_equal(get_val.as.uint32.value, 100);

	/* GET back */
	assert_int_equal(osp_client_get(&client, 1, &dobis, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 100);

	/* Release */
	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ── Test: AARQ rejected ────────────────────────────────────────────────── */

static void test_aarq_rejected(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, 99, NULL);
	osp_client_set_security(&client, &sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_SECURITY);
}

/* ── Test: multi-object dispatch ─────────────────────────────────────────── */

static void test_multi_object(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_client_t client;

	osp_ic_data_t d1, d2;
	osp_ic_data_init(&d1, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_value_t v1 = osp_val_u32(111);
	d1.value = v1;
	osp_ic_data_init(&d2, (osp_obis_t){0, 0, 2, 0, 0, 255});
	osp_value_t v2 = osp_val_u32(222);
	d2.value = v2;
	osp_server_register(&server, osp_ic_data_class(), &d1);
	osp_server_register(&server, osp_ic_data_class(), &d2);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 111);

	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 2, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 222);
}

/* ── Test: client GET/SET/ACTION with-list ─────────────────────────────── */

static void test_client_with_list(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_client_t client;

	osp_obis_t obis_a = {0, 0, 1, 0, 0, 255};
	osp_obis_t obis_b = {0, 0, 2, 0, 0, 255};
	osp_ic_data_t d1, d2;
	osp_ic_data_init(&d1, obis_a);
	d1.value = osp_val_u32(10);
	osp_ic_data_init(&d2, obis_b);
	d2.value = osp_val_u32(20);
	osp_server_register(&server, osp_ic_data_class(), &d1);
	osp_server_register(&server, osp_ic_data_class(), &d2);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_client_attr_ref_t attrs[] = {
	    {1, obis_a, 1},
	    {1, obis_b, 1},
	};
	osp_get_result_item_t get_results[2];
	assert_int_equal(osp_client_get_with_list(&client, attrs, 2, get_results), OSP_OK);
	assert_true(get_results[0].is_data);
	assert_true(get_results[1].is_data);
	assert_int_equal(get_results[0].data.as.uint32.value, 10);
	assert_int_equal(get_results[1].data.as.uint32.value, 20);

	osp_value_t new_vals[] = {osp_val_u32(110), osp_val_u32(220)};
	osp_dar_t set_results[2];
	assert_int_equal(osp_client_set_with_list(&client, attrs, new_vals, 2, set_results), OSP_OK);
	assert_int_equal(set_results[0], OSP_DAR_SUCCESS);
	assert_int_equal(set_results[1], OSP_DAR_SUCCESS);
	assert_int_equal(d1.value.as.uint32.value, 110);
	assert_int_equal(d2.value.as.uint32.value, 220);

	osp_client_method_ref_t methods[] = {
	    {1, obis_a, 1},
	    {1, obis_b, 1},
	};
	osp_action_response_item_t act_results[2];
	assert_int_equal(osp_client_action_with_list(&client, methods, NULL, 2, act_results), OSP_OK);
	assert_int_equal(act_results[0].result, OSP_DAR_SUCCESS);
	assert_int_equal(act_results[1].result, OSP_DAR_SUCCESS);
	assert_int_equal(act_results[0].return_data.as.uint32.value, 110);
	assert_int_equal(act_results[1].return_data.as.uint32.value, 220);
}

/* ── Test: SET+GET roundtrip ─────────────────────────────────────────────── */

static void test_set_get(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_client_t client;

	osp_ic_data_t d;
	osp_ic_data_init(&d, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_value_t zero = osp_val_u32(0);
	d.value = zero;
	osp_server_register(&server, osp_ic_data_class(), &d);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* SET 777 */
	osp_value_t sv = osp_val_u32(777);
	assert_int_equal(osp_client_set(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &sv), OSP_OK);
	osp_value_t gv = d.value;
	assert_int_equal(gv.as.uint32.value, 777);

	/* GET back */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 777);

	/* SET 999 */
	osp_value_t sv2 = osp_val_u32(999);
	assert_int_equal(osp_client_set(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &sv2), OSP_OK);
	osp_value_t result2;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result2), OSP_OK);
	assert_int_equal(result2.as.uint32.value, 999);
}

/* ── Test: HLS GMAC handshake via loopback ───────────────────────────────── */

static void test_hls_handshake(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_value_t init = osp_val_u32(555);
	data_obj.value = init;
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t server_sec;
	osp_sec_context_init(&server_sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	osp_server_set_security(&server, &server_sec);
	setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t client_sec;
	osp_sec_context_init(&client_sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	osp_client_set_security(&client, &client_sec);

	/* Connect with HLS GMAC: AARQ→AARE→pass3→pass4 */
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	assert_true(client.associated);

	/* Verify HLS worked: GET should succeed */
	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 555);

	/* Verify server-side IC tracked correctly */
	assert_int_equal(server.associated, true);
}

#ifdef OSP_HAVE_OPENSSL_GCM

static void run_hls_hash_handshake(osp_auth_mechanism_t mech, const uint8_t client_st[8], const uint8_t server_st[8], uint32_t init_val) {
	mock_crypto_init();
	mock_crypto_init_real_hashes();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(init_val);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t server_sec;
	osp_sec_context_init(&server_sec, OSP_SUITE_0, mech, server_st);
	osp_server_set_security(&server, &server_sec);
	setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t client_sec;
	osp_sec_context_init(&client_sec, OSP_SUITE_0, mech, client_st);
	osp_client_set_security(&client, &client_sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	assert_true(client.associated);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, init_val);
	assert_true(server.associated);
}

static void test_hls_md5_handshake(void **state) {
	(void)state;
	static const uint8_t client_st[8] = {0x43, 0x4C, 0x49, 0x00, 0x00, 0x00, 0x00, 0x01};
	static const uint8_t server_st[8] = {0x53, 0x52, 0x56, 0x00, 0x00, 0x00, 0x00, 0x01};
	run_hls_hash_handshake(OSP_MECH_HLS_MD5, client_st, server_st, 601);
}

static void test_hls_sha1_handshake(void **state) {
	(void)state;
	static const uint8_t client_st[8] = {0x43, 0x4C, 0x49, 0x00, 0x00, 0x00, 0x00, 0x02};
	static const uint8_t server_st[8] = {0x53, 0x52, 0x56, 0x00, 0x00, 0x00, 0x00, 0x02};
	run_hls_hash_handshake(OSP_MECH_HLS_SHA1, client_st, server_st, 602);
}

static void test_hls_sha256_handshake(void **state) {
	(void)state;
	static const uint8_t client_st[8] = {0x43, 0x4C, 0x49, 0x00, 0x00, 0x00, 0x00, 0x03};
	static const uint8_t server_st[8] = {0x53, 0x52, 0x56, 0x00, 0x00, 0x00, 0x00, 0x03};
	run_hls_hash_handshake(OSP_MECH_HLS_SHA256, client_st, server_st, 603);
}

static void test_hls_gost_streebog_handshake(void **state) {
	(void)state;
	static const uint8_t client_st[8] = {0x43, 0x4C, 0x49, 0x00, 0x00, 0x00, 0x00, 0x04};
	static const uint8_t server_st[8] = {0x53, 0x52, 0x56, 0x00, 0x00, 0x00, 0x00, 0x04};
	run_hls_hash_handshake(OSP_MECH_HLS_GOST_STREEBOG, client_st, server_st, 604);
}

#endif /* OSP_HAVE_OPENSSL_GCM */

static void run_hls_gost_sig_handshake(const uint8_t client_st[8], const uint8_t server_st[8], uint32_t init_val) {
	mock_crypto_init();
	mock_transport_pair_t pair;

	static const uint8_t client_sk[32] = {0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	                                      0xBB, 0xBB, 0xAA, 0xAA, 0x99, 0x99, 0x88, 0x88, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77};
	static const uint8_t server_sk[32] = {0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	                                      0xDD, 0xDD, 0xCC, 0xCC, 0xAA, 0xAA, 0xBB, 0xBB};
	uint8_t client_pk[64], server_pk[64];
	assert_int_equal(osp_gost3410_public_key(client_sk, client_pk), 0);
	assert_int_equal(osp_gost3410_public_key(server_sk, server_pk), 0);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(init_val);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t server_sec;
	osp_sec_context_init(&server_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, server_st);
	memcpy(server_sec.signing_key, server_sk, sizeof(server_sk));
	server_sec.signing_key_len = (uint8_t)sizeof(server_sk);
	memcpy(server_sec.peer_public_key, client_pk, sizeof(client_pk));
	server_sec.peer_public_key_len = (uint8_t)sizeof(client_pk);
	osp_server_set_security(&server, &server_sec);
	setup(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t client_sec;
	osp_sec_context_init(&client_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, client_st);
	memcpy(client_sec.signing_key, client_sk, sizeof(client_sk));
	client_sec.signing_key_len = (uint8_t)sizeof(client_sk);
	memcpy(client_sec.peer_public_key, server_pk, sizeof(server_pk));
	client_sec.peer_public_key_len = (uint8_t)sizeof(server_pk);
	osp_client_set_security(&client, &client_sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	assert_true(client.associated);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, init_val);
	assert_true(server.associated);
}

static void test_hls_gost_sig_handshake(void **state) {
	(void)state;
	static const uint8_t client_st[8] = {0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D};
	static const uint8_t server_st[8] = {0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E};
	run_hls_gost_sig_handshake(client_st, server_st, 605);
}

/* ── Test: client ACTION (disconnect control) ────────────────────────────── */

static void test_client_action(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_disconnect_control_t dc;
	osp_ic_disconnect_control_init(&dc, (osp_obis_t){0, 0, 96, 3, 10, 255});
	dc.output_state = 1;
	osp_server_register(&server, osp_ic_disconnect_control_class(), &dc);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	osp_obis_t obis = {0, 0, 96, 3, 10, 255};
	assert_int_equal(osp_client_action(&client, 70, &obis, 1, NULL, &result), OSP_OK);
	assert_int_equal(dc.output_state, 0);
	assert_int_equal(result.tag, OSP_TAG_NULL);
}

/* ── Test: client release + disconnect ───────────────────────────────────── */

static void test_client_release_disconnect(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);
	assert_true(client.associated);

	assert_int_equal(osp_client_release(&client), OSP_OK);
	assert_false(client.associated);

	assert_int_equal(osp_client_disconnect(&client), OSP_OK);
	assert_int_equal(osp_client_disconnect(NULL), OSP_ERR_INVALID);
}

/* ── Test: large GET via block transfer ──────────────────────────────────── */

static osp_value_t make_octets(uint8_t fill, uint32_t len) {
	osp_value_t v;
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = len;
	memset(v.as.octetstring.data, fill, len);
	return v;
}

static osp_buf_t make_rbuf(uint8_t *mem, uint32_t len) {
	osp_buf_t b;
	osp_buf_init(&b, mem, len);
	b.wr = len;
	return b;
}

static osp_err_t loopback_exchange(mock_transport_pair_t *pair, osp_server_t *server, const uint8_t *tx, uint32_t tx_len,
                                  uint8_t *rx, uint32_t rx_size, uint32_t *rx_len) {
	osp_err_t r = mock_loopback_send(pair, server, tx, tx_len);
	if (r != OSP_OK) {
		return r;
	}
	return mock_recv_from_peer(&pair->client_rx, rx, rx_size, rx_len, 0);
}

static void test_get_block_transfer(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_server_set_max_pdu(&server, 32);

	osp_obis_t obis = {0, 0, 0x80, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = make_octets(0xAB, 200);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(result.as.octetstring.len, 200);
	for (uint32_t i = 0; i < 200; i++) {
		assert_int_equal(result.as.octetstring.data[i], 0xAB);
	}
}

/* ── Test: GET large value via GBT (not service-specific datablock) ─────── */

static void test_gbt_get_large_value(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_server_enable_gbt(&server, 48);

	osp_obis_t obis = {0, 0, 0x81, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = make_octets(0xCD, 180);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	osp_client_enable_gbt(&client, 48);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(result.as.octetstring.len, 180);
	for (uint32_t i = 0; i < 180; i++) {
		assert_int_equal(result.as.octetstring.data[i], 0xCD);
	}
}

/* ── Test: GET large value via streaming GBT confirmed mode (window=1) ──── */

static void test_gbt_get_large_value_streaming_confirmed(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_server_enable_gbt(&server, 48);
	osp_server_set_gbt_window(&server, 1);
	osp_server_set_gbt_streaming(&server, true);

	osp_obis_t obis = {0, 0, 0x82, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = make_octets(0xDE, 180);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	mock_transport_enable_gbt_ack_pump(&pair, true);
	osp_client_enable_gbt(&client, 48);
	osp_client_set_gbt_window(&client, 1);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(result.as.octetstring.len, 180);
	for (uint32_t i = 0; i < 180; i++) {
		assert_int_equal(result.as.octetstring.data[i], 0xDE);
	}
}

/* ── Test: SET reassembled from datablocks ───────────────────────────────── */

static void test_set_block_transfer(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {1, 0, 1, 8, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = osp_val_u32(0);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t to_write = make_octets(0x5A, 40);
	uint8_t value_bytes[64];
	osp_buf_t w;
	osp_buf_init(&w, value_bytes, sizeof(value_bytes));
	assert_int_equal(osp_value_write(&w, &to_write), OSP_OK);
	const uint32_t value_len = w.wr;

	osp_set_request_t req1;
	memset(&req1, 0, sizeof(req1));
	req1.type = OSP_SET_WITH_FIRST_DATABLOCK;
	req1.invoke_id_priority = OSP_IIDP(1, 0);
	req1.as.first_datablock.attr.class_id = 1;
	req1.as.first_datablock.attr.instance_id = obis;
	req1.as.first_datablock.attr.attribute_id = 1;
	req1.as.first_datablock.datablock.last_block = false;
	req1.as.first_datablock.datablock.block_number = 1;
	req1.as.first_datablock.datablock.raw_data_len = 20;
	memcpy(req1.as.first_datablock.datablock.raw_data, value_bytes, 20);

	uint8_t tx[128];
	uint8_t rx[128];
	uint32_t rx_len;
	osp_buf_init(&w, tx, sizeof(tx));
	assert_int_equal(osp_set_request_encode(&w, &req1), 0);
	assert_int_equal(loopback_exchange(&pair, &server, tx, w.wr, rx, sizeof(rx), &rx_len), OSP_OK);

	osp_set_response_t resp;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, rx, rx_len);
	rbuf.wr = rx_len;
	assert_int_equal(osp_set_response_decode(&rbuf, &resp), 0);
	assert_int_equal(resp.type, OSP_SET_RESP_DATABLOCK);
	assert_int_equal(resp.as.datablock.block_number, 1);

	osp_set_request_t req2;
	memset(&req2, 0, sizeof(req2));
	req2.type = OSP_SET_WITH_DATABLOCK;
	req2.invoke_id_priority = OSP_IIDP(1, 0);
	req2.as.datablock.datablock.last_block = true;
	req2.as.datablock.datablock.block_number = 2;
	req2.as.datablock.datablock.raw_data_len = value_len - 20;
	memcpy(req2.as.datablock.datablock.raw_data, &value_bytes[20], value_len - 20);

	osp_buf_init(&w, tx, sizeof(tx));
	assert_int_equal(osp_set_request_encode(&w, &req2), 0);
	assert_int_equal(loopback_exchange(&pair, &server, tx, w.wr, rx, sizeof(rx), &rx_len), OSP_OK);

	osp_buf_init(&rbuf, rx, rx_len);
	rbuf.wr = rx_len;
	assert_int_equal(osp_set_response_decode(&rbuf, &resp), 0);
	assert_int_equal(resp.type, OSP_SET_RESP_LAST_DATABLOCK);
	assert_int_equal(resp.as.last_datablock.result, OSP_DAR_SUCCESS);
	assert_int_equal(resp.as.last_datablock.block_number, 2);

	osp_value_t got;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &got), OSP_OK);
	assert_int_equal(got.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(got.as.octetstring.len, 40);
	for (uint32_t i = 0; i < 40; i++) {
		assert_int_equal(got.as.octetstring.data[i], 0x5A);
	}
}

static void test_client_set_via_blocks(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {1, 0, 2, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = osp_val_u32(0);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t to_write = make_octets(0x5A, 40);
	assert_int_equal(osp_client_set(&client, 1, &obis, 1, &to_write), OSP_OK);

	osp_value_t got;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &got), OSP_OK);
	assert_int_equal(got.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(got.as.octetstring.len, 40);
}

static void test_action_param_block_invoke(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {0, 0, 44, 0, 0, 255};
	osp_ic_image_transfer_t img;
	osp_ic_image_transfer_init(&img, obis);
	img.image_transfer_status = OSP_IMAGE_IDLE;
	osp_server_register(&server, osp_ic_image_transfer_class(), &img);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t big_param = make_octets(0xAB, 80);
	osp_value_t result;
	assert_int_equal(osp_client_action(&client, 18, &obis, 2, &big_param, &result), OSP_OK);
	assert_int_equal(img.image_transfer_status, OSP_IMAGE_TRANSFER_RUNNING);
}

static void test_action_return_param_blocks(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	osp_server_set_max_pdu(&server, 32);

	osp_obis_t obis = {0, 0, 0x81, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = make_octets(0xCD, 120);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_action(&client, 1, &obis, 1, NULL, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(result.as.octetstring.len, 120);
	for (uint32_t i = 0; i < 120; i++) {
		assert_int_equal(result.as.octetstring.data[i], 0xCD);
	}
}

static void test_compact_data_capture_via_dispatcher(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis_a = {0, 0, 10, 0, 0, 255};
	osp_obis_t obis_b = {0, 0, 11, 0, 0, 255};
	osp_ic_data_t data_a, data_b;
	osp_ic_data_init(&data_a, obis_a);
	osp_ic_data_init(&data_b, obis_b);
	data_a.value = osp_val_u8(1);
	data_b.value = osp_val_u16(0x0102);
	osp_server_register(&server, osp_ic_data_class(), &data_a);
	osp_server_register(&server, osp_ic_data_class(), &data_b);

	osp_obis_t cd_obis = {0, 0, 99, 0, 0, 255};
	osp_ic_compact_data_t cd;
	osp_ic_compact_data_init(&cd, cd_obis);
	cd.capture_objects.items[0] = (osp_capture_object_t){1, obis_a, 1, 0, {0}};
	cd.capture_objects.items[1] = (osp_capture_object_t){1, obis_b, 1, 0, {0}};
	cd.capture_objects.count = 2;
	osp_server_register(&server, osp_ic_compact_data_class(), &cd);

	osp_value_t ignored = osp_val_null();
	assert_int_equal(osp_dispatcher_action(&server.dispatcher, 62, &cd_obis, 2, NULL, &ignored), OSP_OK);

	osp_value_t buf_val;
	assert_int_equal(osp_ic_compact_data_class()->get_attr(&cd, 2, &buf_val), OSP_OK);
	assert_int_equal(buf_val.tag, OSP_TAG_OCTETSTRING);

	osp_buf_t r = make_rbuf(buf_val.as.octetstring.data, buf_val.as.octetstring.len);
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&r, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_ARRAY);
	assert_int_equal(decoded.as.array.elements.count, 1);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[0].as.uint8.value, 1);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[1].as.uint16.value, 0x0102);
}

static void test_push_compact_notification(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis_a = {0, 0, 20, 0, 0, 255};
	osp_obis_t obis_b = {0, 0, 21, 0, 0, 255};
	osp_ic_data_t data_a, data_b;
	osp_ic_data_init(&data_a, obis_a);
	osp_ic_data_init(&data_b, obis_b);
	data_a.value = osp_val_u8(5);
	data_b.value = osp_val_u16(0x0A0B);
	osp_server_register(&server, osp_ic_data_class(), &data_a);
	osp_server_register(&server, osp_ic_data_class(), &data_b);

	osp_obis_t cd_obis = {0, 0, 98, 0, 0, 255};
	osp_ic_compact_data_t cd;
	osp_ic_compact_data_init(&cd, cd_obis);
	cd.capture_objects.items[0] = (osp_capture_object_t){1, obis_a, 1, 0, {0}};
	cd.capture_objects.items[1] = (osp_capture_object_t){1, obis_b, 1, 0, {0}};
	cd.capture_objects.count = 2;
	osp_server_register(&server, osp_ic_compact_data_class(), &cd);

	osp_obis_t push_obis = {0, 0, 25, 0, 0, 255};
	osp_ic_push_setup_t push;
	osp_ic_push_setup_init(&push, push_obis);
	push.push_object_list[0] = (osp_push_object_t){62, cd_obis, 2};
	push.push_object_count = 1;
	osp_server_register(&server, osp_ic_push_setup_class(), &push);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_action(&client, 40, &push_obis, 1, NULL, &result), OSP_OK);

	osp_data_notification_t dn;
	assert_int_equal(osp_client_recv_data_notification(&client, &dn, 5000), OSP_OK);
	assert_int_equal(dn.notification_body.tag, OSP_TAG_OCTETSTRING);

	osp_buf_t r = make_rbuf(dn.notification_body.as.octetstring.data, dn.notification_body.as.octetstring.len);
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&r, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_ARRAY);
	assert_int_equal(decoded.as.array.elements.count, 1);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[0].as.uint8.value, 5);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[1].as.uint16.value, 0x0A0B);
}

static void test_server_event_notification(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {0, 0, 0x60, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = osp_val_u32(77);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_event_notification_t ev = {0};
	ev.attribute.class_id = 1;
	ev.attribute.instance_id = obis;
	ev.attribute.attribute_id = 1;
	ev.value = osp_val_u32(77);

	assert_int_equal(osp_server_send_event_notification(&server, &ev), OSP_OK);

	osp_event_notification_t recv_ev;
	assert_int_equal(osp_client_recv_event_notification(&client, &recv_ev, 5000), OSP_OK);
	assert_int_equal(recv_ev.attribute.class_id, 1);
	assert_int_equal(recv_ev.value.as.uint32.value, 77);
}

#ifdef OSP_HAVE_OPENSSL_GCM

static void setup_glo_cipher_contexts(osp_sec_context_t *client_tx, osp_sec_context_t *client_rx, osp_sec_context_t *server_tx,
                                      osp_sec_context_t *server_rx) {
	static const uint8_t ek[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
	static const uint8_t ak[16] = {0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF};
	static const uint8_t client_st[8] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E};
	static const uint8_t server_st[8] = {0x53, 0x52, 0x56, 0x00, 0x00, 0x00, 0x00, 0x01};

	osp_sec_context_init(client_tx, OSP_SUITE_0, OSP_MECH_LOWEST, client_st);
	client_tx->policy = OSP_POLICY_ENCR_AUTH;
	memcpy(client_tx->guek, ek, 16);
	memcpy(client_tx->gak, ak, 16);
	client_tx->invocation_counter = 1;

	osp_sec_context_init(client_rx, OSP_SUITE_0, OSP_MECH_LOWEST, server_st);
	client_rx->policy = OSP_POLICY_ENCR_AUTH;
	memcpy(client_rx->guek, ek, 16);
	memcpy(client_rx->gak, ak, 16);

	osp_sec_context_init(server_tx, OSP_SUITE_0, OSP_MECH_LOWEST, server_st);
	server_tx->policy = OSP_POLICY_ENCR_AUTH;
	memcpy(server_tx->guek, ek, 16);
	memcpy(server_tx->gak, ak, 16);
	server_tx->invocation_counter = 1;

	osp_sec_context_init(server_rx, OSP_SUITE_0, OSP_MECH_LOWEST, client_st);
	server_rx->policy = OSP_POLICY_ENCR_AUTH;
	memcpy(server_rx->guek, ek, 16);
	memcpy(server_rx->gak, ak, 16);
}

static void test_glo_get_ciphered(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();

	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {0, 0, 0x90, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = osp_val_u32(4242);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t client_tx, client_rx, server_tx, server_rx;
	setup_glo_cipher_contexts(&client_tx, &client_rx, &server_tx, &server_rx);
	osp_server_set_ciphering(&server, &server_tx, &server_rx);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	osp_client_set_ciphering(&client, &client_tx, &client_rx);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 4242);
}

static void test_ded_get_ciphered(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();

	mock_transport_pair_t pair;
	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_obis_t obis = {0, 0, 0x90, 0, 0, 255};
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, obis);
	data_obj.value = osp_val_u32(9001);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t client_tx, client_rx, server_tx, server_rx;
	setup_glo_cipher_contexts(&client_tx, &client_rx, &server_tx, &server_rx);
	osp_server_set_ciphering(&server, &server_tx, &server_rx);

	static const uint8_t dek[16] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF};

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	osp_client_t client;
	make_pair(&pair, &server, &client);
	osp_client_set_ciphering(&client, &client_tx, &client_rx);
	osp_client_set_dedicated_key(&client, dek, 16);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	assert_int_equal(osp_client_get(&client, 1, &obis, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 9001);
}

#endif /* OSP_HAVE_OPENSSL_GCM */

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_aarq_hls_get), cmocka_unit_test(test_aarq_rejected), cmocka_unit_test(test_multi_object),
	    cmocka_unit_test(test_client_with_list),
	    cmocka_unit_test(test_set_get),      	    cmocka_unit_test(test_hls_handshake),
#ifdef OSP_HAVE_OPENSSL_GCM
	    cmocka_unit_test(test_hls_md5_handshake),
	    cmocka_unit_test(test_hls_sha1_handshake),
	    cmocka_unit_test(test_hls_sha256_handshake),
	    cmocka_unit_test(test_hls_gost_streebog_handshake),
#endif
	    cmocka_unit_test(test_hls_gost_sig_handshake),
	    cmocka_unit_test(test_client_action),
	    cmocka_unit_test(test_client_release_disconnect),
	    cmocka_unit_test(test_get_block_transfer),
	    cmocka_unit_test(test_gbt_get_large_value),
	    cmocka_unit_test(test_gbt_get_large_value_streaming_confirmed),
	    cmocka_unit_test(test_set_block_transfer),
	    cmocka_unit_test(test_client_set_via_blocks),
	    cmocka_unit_test(test_action_param_block_invoke),
	    cmocka_unit_test(test_action_return_param_blocks),
	    cmocka_unit_test(test_compact_data_capture_via_dispatcher),
	    cmocka_unit_test(test_push_compact_notification),
	    cmocka_unit_test(test_server_event_notification),
#ifdef OSP_HAVE_OPENSSL_GCM
	    cmocka_unit_test(test_glo_get_ciphered),
	    cmocka_unit_test(test_ded_get_ciphered),
#endif
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
