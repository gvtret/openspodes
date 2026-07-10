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
#include "../src/ic/data.h"
#include "../src/ic/disconnect_control.h"
#include "../src/ic/image_transfer.h"
#include "../src/security/security.h"
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

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_aarq_hls_get), cmocka_unit_test(test_aarq_rejected), cmocka_unit_test(test_multi_object),
	    cmocka_unit_test(test_set_get),      	    cmocka_unit_test(test_hls_handshake),
	    cmocka_unit_test(test_client_action),
	    cmocka_unit_test(test_client_release_disconnect),
	    cmocka_unit_test(test_get_block_transfer),
	    cmocka_unit_test(test_set_block_transfer),
	    cmocka_unit_test(test_client_set_via_blocks),
	    cmocka_unit_test(test_action_param_block_invoke),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
