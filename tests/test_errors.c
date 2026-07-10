/**
 * test_errors.c — Error-path and edge-case tests
 *
 * Covers codec, transport, client, and server failure branches.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "codec/codec.h"
#include "codec/serialize.h"
#include "service/service.h"
#include "transport/transport.h"
#include "client/client.h"
#include "server/server.h"
#include "ic/data.h"
#include "mock_transport.h"
#include "mock_crypto.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Codec error paths
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_ber_octet_string_errors(void **state) {
	(void)state;
	uint8_t mem[16];
	osp_buf_t buf;
	uint8_t out[8];
	uint32_t out_len;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_read_octet_string(NULL, out, sizeof(out), &out_len), OSP_ERR_INVALID);

	/* Wrong BER tag (INTEGER instead of OCTET STRING) */
	mem[0] = 0x02;
	mem[1] = 0x01;
	mem[2] = 0x05;
	osp_buf_init(&buf, mem, 3);
	buf.wr = 3;
	assert_int_equal(osp_ber_read_octet_string(&buf, out, sizeof(out), &out_len), OSP_ERR_INVALID);

	/* Truncated payload */
	mem[0] = 0x04;
	mem[1] = 0x03;
	mem[2] = 0xAA;
	osp_buf_init(&buf, mem, 3);
	buf.wr = 3;
	assert_int_equal(osp_ber_read_octet_string(&buf, out, sizeof(out), &out_len), OSP_ERR_INVALID);

	/* Output buffer too small */
	mem[0] = 0x04;
	mem[1] = 0x04;
	mem[2] = 0xAA;
	mem[3] = 0xBB;
	mem[4] = 0xCC;
	mem[5] = 0xDD;
	osp_buf_init(&buf, mem, 6);
	buf.wr = 6;
	assert_int_equal(osp_ber_read_octet_string(&buf, out, 2, &out_len), OSP_ERR_NOMEM);
}

static void test_ber_uint_and_write_errors(void **state) {
	(void)state;
	uint8_t mem[4];
	osp_buf_t buf;
	uint32_t val;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_read_uint(&buf, &val), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_read_uint(NULL, &val), OSP_ERR_INVALID);

	osp_buf_init(&buf, mem, 1);
	buf.wr = 1;
	mem[0] = 0x42;
	assert_int_equal(osp_ber_read_uint(&buf, &val), OSP_OK);
	assert_int_equal(val, 0x42);

	osp_buf_init(&buf, mem, 1);
	assert_int_equal(osp_ber_write_tag(NULL, 0, false, 1), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_write_length(NULL, 1), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_write_uint(NULL, 1), OSP_ERR_INVALID);

	osp_buf_init(&buf, mem, 0);
	assert_int_equal(osp_ber_write_tag(&buf, 0, false, 1), OSP_ERR_NOMEM);
}

static void test_axdr_read_errors(void **state) {
	(void)state;
	uint8_t mem[8];
	osp_buf_t buf;
	uint8_t out[4];
	uint32_t out_len;
	uint8_t tag;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_axdr_read_tag(&buf, &tag), OSP_ERR_INVALID);
	assert_int_equal(osp_axdr_read_u16(&buf, NULL), OSP_ERR_INVALID);
	assert_int_equal(osp_axdr_read_bool(&buf, NULL), OSP_ERR_INVALID);

	/* AXDR octet string with wrong tag */
	mem[0] = 0x07;
	mem[1] = 0x00;
	mem[2] = 0x00;
	mem[3] = 0x00;
	mem[4] = 0x01;
	mem[5] = 0xAA;
	osp_buf_init(&buf, mem, 6);
	buf.wr = 6;
	assert_int_equal(osp_axdr_read_octet_string(&buf, out, sizeof(out), &out_len), OSP_ERR_INVALID);
}

static void test_value_read_errors(void **state) {
	(void)state;
	uint8_t mem[8];
	osp_buf_t buf;
	osp_value_t val;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_value_read(NULL, &val), OSP_ERR_INVALID);
	assert_int_equal(osp_value_read(&buf, NULL), OSP_ERR_INVALID);

	mem[0] = 0xFF; /* unsupported COSEM tag */
	osp_buf_init(&buf, mem, 1);
	buf.wr = 1;
	assert_int_equal(osp_value_read(&buf, &val), OSP_ERR_UNSUPPORTED);

	mem[0] = OSP_TAG_VISIBLESTRING;
	mem[1] = 0x05; /* length 5 but only 2 bytes follow */
	mem[2] = 'A';
	mem[3] = 'B';
	osp_buf_init(&buf, mem, 4);
	buf.wr = 4;
	assert_int_equal(osp_value_read(&buf, &val), OSP_ERR_INVALID);
}

static void test_obis_errors(void **state) {
	(void)state;
	uint8_t mem[4];
	osp_buf_t buf;
	osp_obis_t obis;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_obis_read(&buf, &obis), OSP_ERR_INVALID);
	assert_int_equal(osp_obis_write(&buf, NULL), OSP_ERR_INVALID);

	osp_buf_init(&buf, mem, 2);
	buf.wr = 2;
	assert_int_equal(osp_obis_write(&buf, &(osp_obis_t){1, 2, 3, 4, 5, 6}), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Transport error paths
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wrapper_errors(void **state) {
	(void)state;
	osp_wrapper_header_t hdr;
	const uint8_t *apdu;
	uint32_t apdu_len;

	assert_int_equal(osp_wrapper_decode(NULL, 8, &hdr, &apdu, &apdu_len), OSP_ERR_INVALID);
	assert_int_equal(osp_wrapper_encode(1, 2, NULL, 1, (uint8_t[8]){0}, 8, &apdu_len), OSP_ERR_INVALID);

	uint8_t short_buf[4] = {0};
	assert_int_equal(osp_wrapper_decode(short_buf, sizeof(short_buf), &hdr, &apdu, &apdu_len), OSP_ERR_INVALID);

	uint8_t bad_ver[10] = {0x00, 0x99, 0, 0, 0, 0, 0, 1, 0xC0};
	assert_int_equal(osp_wrapper_decode(bad_ver, sizeof(bad_ver), &hdr, &apdu, &apdu_len), OSP_ERR_INVALID);

	uint8_t bad_len[10] = {0x00, 0x01, 0, 0, 0, 0, 0, 0xFF, 0xC0};
	assert_int_equal(osp_wrapper_decode(bad_len, sizeof(bad_len), &hdr, &apdu, &apdu_len), OSP_ERR_INVALID);
}

static void test_hdlc_deframe_errors(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;

	assert_int_equal(osp_hdlc_deframe(NULL, 8, &frame), OSP_ERR_INVALID);

	uint8_t no_flags[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
	assert_int_equal(osp_hdlc_deframe(no_flags, sizeof(no_flags), &frame), OSP_ERR_INVALID);

	uint8_t bad_fcs[] = {0x7E, 0x0A, 0x00, 0x03, 0x93, 0x01, 0x00, 0x7E};
	assert_int_equal(osp_hdlc_deframe(bad_fcs, sizeof(bad_fcs), &frame), OSP_ERR_INVALID);
}

static void test_transport_apdu_errors(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	uint8_t apdu[] = {0xC0, 0x01, 0x41};
	uint8_t rx[64];
	uint32_t rx_len;

	assert_int_equal(osp_transport_send_apdu(NULL, OSP_FRAMING_NONE, apdu, sizeof(apdu)), OSP_ERR_INVALID);
	assert_int_equal(osp_transport_recv_apdu(NULL, OSP_FRAMING_NONE, rx, sizeof(rx), &rx_len, 100), OSP_ERR_INVALID);
	assert_int_equal(osp_transport_send_apdu(&pair.client_transport, OSP_FRAMING_NONE, apdu, sizeof(apdu)), OSP_OK);
	assert_int_equal(osp_transport_recv_apdu(&pair.server_transport, OSP_FRAMING_NONE, rx, sizeof(rx), &rx_len, 100), OSP_OK);
	assert_int_equal(rx_len, sizeof(apdu));
}

static void test_transport_hdlc_framing(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	const uint8_t apdu[] = {0xC0, 0x01, 0x41, 0x00, 0x03, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x02, 0x00};
	uint8_t rx[128];
	uint32_t rx_len;

	assert_int_equal(osp_transport_send_apdu(&pair.client_transport, OSP_FRAMING_HDLC, apdu, sizeof(apdu)), OSP_OK);
	assert_int_equal(osp_transport_recv_apdu(&pair.server_transport, OSP_FRAMING_HDLC, rx, sizeof(rx), &rx_len, 100), OSP_OK);
	assert_int_equal(rx_len, sizeof(apdu));
	assert_memory_equal(rx, apdu, sizeof(apdu));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Client / server error paths (integration-style)
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_server_t *g_server = NULL;

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	mock_send_to_peer(&p->server_rx, data, len);
	if (g_server) {
		osp_server_accept(g_server, 0);
	}
	return OSP_OK;
}

static osp_err_t loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static osp_err_t fail_send(void *ctx, const uint8_t *data, uint32_t len) {
	(void)ctx;
	(void)data;
	(void)len;
	return OSP_ERR_NOMEM;
}

static void test_client_not_connected(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_client_t client;
	assert_int_equal(osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE), OSP_OK);
	assert_int_equal(osp_client_init(NULL, &pair.client_transport, OSP_FRAMING_NONE), OSP_ERR_INVALID);

	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	osp_value_t val;
	assert_int_equal(osp_client_get(&client, 1, &obis, 2, &val), OSP_ERR_INVALID);
	assert_int_equal(osp_client_set(&client, 1, &obis, 2, &val), OSP_ERR_INVALID);
	assert_int_equal(osp_client_action(&client, 1, &obis, 1, NULL, &val), OSP_ERR_INVALID);
}

static void test_client_get_not_found(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	pair.client_transport.send = loopback_send;
	pair.client_transport.recv = loopback_recv;
	pair.client_transport.ctx = &pair;
	g_server = &server;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_client_set_security(&client, &sec);
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_value_t result;
	osp_obis_t missing = {9, 9, 9, 9, 9, 9};
	assert_int_equal(osp_client_get(&client, 1, &missing, 2, &result), OSP_ERR_NOT_FOUND);
	g_server = NULL;
}

static void test_client_transport_failure(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);
	pair.client_transport.send = fail_send;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_NOMEM);
}

static void test_server_invalid_and_unsupported(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_server_t server;
	assert_int_equal(osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE), OSP_OK);
	assert_int_equal(osp_server_init(NULL, &pair.server_transport, OSP_FRAMING_NONE), OSP_ERR_INVALID);
	assert_int_equal(osp_server_register(&server, osp_ic_data_class(), NULL), OSP_ERR_INVALID);

	/* GET without association */
	uint8_t mem[32];
	osp_buf_t buf = {mem, sizeof(mem), 0, 0};
	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = 0x41;
	req.as.normal.attr.class_id = 1;
	req.as.normal.attr.instance_id = (osp_obis_t){0, 0, 1, 0, 0, 255};
	req.as.normal.attr.attribute_id = 2;
	osp_get_request_encode(&buf, &req);
	mock_send_to_peer(&pair.server_rx, mem, buf.wr);
	assert_int_equal(osp_server_accept(&server, 5000), OSP_ERR_INVALID);

	/* Associate first */
	server.associated = true;

	/* Unsupported service tag */
	const uint8_t bad_svc[] = {0xD0, 0x01, 0x41};
	mock_send_to_peer(&pair.server_rx, bad_svc, sizeof(bad_svc));
	assert_int_equal(osp_server_accept(&server, 5000), OSP_ERR_UNSUPPORTED);
}

static void test_server_init_errors(void **state) {
	(void)state;
	osp_server_t server;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	assert_int_equal(osp_server_accept(NULL, 100), OSP_ERR_INVALID);

	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	assert_int_equal(osp_server_accept(&server, 0), OSP_ERR_TIMEOUT);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_ber_octet_string_errors),
	    cmocka_unit_test(test_ber_uint_and_write_errors),
	    cmocka_unit_test(test_axdr_read_errors),
	    cmocka_unit_test(test_value_read_errors),
	    cmocka_unit_test(test_obis_errors),
	    cmocka_unit_test(test_wrapper_errors),
	    cmocka_unit_test(test_hdlc_deframe_errors),
	    cmocka_unit_test(test_transport_apdu_errors),
	    cmocka_unit_test(test_transport_hdlc_framing),
	    cmocka_unit_test(test_client_not_connected),
	    cmocka_unit_test(test_client_get_not_found),
	    cmocka_unit_test(test_client_transport_failure),
	    cmocka_unit_test(test_server_invalid_and_unsupported),
	    cmocka_unit_test(test_server_init_errors),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
