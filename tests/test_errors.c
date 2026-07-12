/**
 * test_errors.c — Error-path and edge-case tests
 *
 * Covers codec, transport, client, and server failure branches.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
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
#include "security/security.h"

static void hex_to_bytes(const char *hex, uint8_t *out, uint32_t *out_len) {
	uint32_t n = 0;
	while (hex[0] && hex[1]) {
		char pair[3] = {hex[0], hex[1], 0};
		out[n++] = (uint8_t)strtoul(pair, NULL, 16);
		hex += 2;
	}
	*out_len = n;
}

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

static void test_ber_length_and_octet_string(void **state) {
	(void)state;
	uint8_t mem[512];
	osp_buf_t buf;
	uint32_t len;
	uint32_t enc_len;
	uint8_t out[64];

	/* BER length 0x81 (one-byte long form) */
	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_write_length(&buf, 200), OSP_OK);
	enc_len = buf.wr;
	osp_buf_init(&buf, mem, enc_len);
	buf.wr = enc_len;
	assert_int_equal(osp_ber_read_length(&buf, &len), OSP_OK);
	assert_int_equal(len, 200);

	/* BER length 0x82 (two-byte long form) */
	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_write_length(&buf, 300), OSP_OK);
	enc_len = buf.wr;
	osp_buf_init(&buf, mem, enc_len);
	buf.wr = enc_len;
	assert_int_equal(osp_ber_read_length(&buf, &len), OSP_OK);
	assert_int_equal(len, 300);

	/* Truncated long form (0x83 needs 3 content bytes) */
	mem[0] = 0x83;
	osp_buf_init(&buf, mem, 1);
	buf.wr = 1;
	assert_int_equal(osp_ber_read_length(&buf, &len), OSP_ERR_INVALID);

	/* Truncated long tag number */
	mem[0] = 0x1F;
	osp_buf_init(&buf, mem, 1);
	buf.wr = 1;
	osp_ber_tag_t tag;
	assert_int_equal(osp_ber_read_tag(&buf, &tag), OSP_ERR_INVALID);

	/* Octet string roundtrip (out_len optional) */
	const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_write_octet_string(&buf, payload, sizeof(payload)), OSP_OK);
	enc_len = buf.wr;
	osp_buf_init(&buf, mem, enc_len);
	buf.wr = enc_len;
	assert_int_equal(osp_ber_read_octet_string(&buf, out, sizeof(out), NULL), OSP_OK);

	osp_buf_init(&buf, mem, enc_len);
	buf.wr = enc_len;
	assert_int_equal(osp_ber_read_octet_string(&buf, out, sizeof(out), &len), OSP_OK);
	assert_int_equal(len, sizeof(payload));
	assert_memory_equal(out, payload, sizeof(payload));

	/* Write octet string NOMEM */
	osp_buf_init(&buf, mem, 2);
	assert_int_equal(osp_ber_write_octet_string(&buf, payload, sizeof(payload)), OSP_ERR_NOMEM);
}

static void test_axdr_octet_string_roundtrip(void **state) {
	(void)state;
	uint8_t mem[32];
	osp_buf_t buf;
	uint8_t out[8];
	const uint8_t payload[] = {0x01, 0x02, 0x03};

	mem[0] = OSP_AXDR_OCTETSTRING;
	mem[1] = (uint8_t)sizeof(payload);
	memcpy(&mem[2], payload, sizeof(payload));

	osp_buf_init(&buf, mem, sizeof(mem));
	buf.wr = 2 + sizeof(payload);
	assert_int_equal(osp_axdr_read_octet_string(&buf, out, sizeof(out), NULL), OSP_OK);

	osp_buf_init(&buf, mem, buf.wr);
	buf.wr = 2 + sizeof(payload);
	uint32_t out_len;
	assert_int_equal(osp_axdr_read_octet_string(&buf, out, sizeof(out), &out_len), OSP_OK);
	assert_int_equal(out_len, sizeof(payload));
	assert_memory_equal(out, payload, sizeof(payload));
}

static void test_serialize_access_and_objects(void **state) {
	(void)state;
	uint8_t mem[256];
	osp_buf_t w, r;

	osp_access_right_t ar = {0};
	ar.attr_count = 1;
	ar.attr_items[0].attribute_id = 2;
	ar.attr_items[0].access_mode = OSP_ACCESS_READ_WRITE;
	ar.method_count = 1;
	ar.method_items[0].method_id = 1;
	ar.method_items[0].access_mode = OSP_METHOD_ACCESS;

	assert_int_equal(osp_access_right_read(NULL, &ar), OSP_ERR_INVALID);
	assert_int_equal(osp_access_right_write(NULL, &ar), OSP_ERR_INVALID);

	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_access_right_write(&w, &ar), OSP_OK);
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_access_right_t ar2 = {0};
	assert_int_equal(osp_access_right_read(&r, &ar2), OSP_OK);
	assert_int_equal(ar2.attr_count, 1);
	assert_int_equal(ar2.attr_items[0].attribute_id, 2);
	assert_int_equal(ar2.method_count, 1);
	assert_int_equal(ar2.method_items[0].method_id, 1);

	osp_object_list_element_t elem = {
	    .class_id = 3,
	    .version = 0,
	    .logical_name = {0, 0, 1, 0, 0, 255},
	    .access_rights = ar,
	};
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_object_list_element_write(&w, &elem), OSP_OK);
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_object_list_element_t elem2;
	assert_int_equal(osp_object_list_element_read(&r, &elem2), OSP_OK);
	assert_int_equal(elem2.class_id, 3);
	assert_true(osp_obis_eq(&elem2.logical_name, &elem.logical_name));

	osp_capture_object_t co = {
	    .class_id = 1,
	    .logical_name = {0, 0, 96, 1, 0, 255},
	    .attribute_index = 2,
	    .data_index = 0,
	};
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_capture_object_write(&w, &co), OSP_OK);
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_capture_object_t co2;
	assert_int_equal(osp_capture_object_read(&r, &co2), OSP_OK);
	assert_int_equal(co2.class_id, 1);
	assert_int_equal(co2.attribute_index, 2);

	osp_value_definition_t vd = {
	    .class_id = 3,
	    .logical_name = {1, 0, 0, 9, 1, 255},
	    .attribute_index = 2,
	};
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_value_definition_write(&w, &vd), OSP_OK);
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_value_definition_t vd2;
	assert_int_equal(osp_value_definition_read(&r, &vd2), OSP_OK);
	assert_int_equal(vd2.class_id, 3);
	assert_int_equal(vd2.attribute_index, 2);
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

static void test_loopback_send_propagates_server_error(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	server.associated = true;

	const uint8_t bad_svc[] = {0xD0, 0x01, 0x41};
	assert_int_equal(mock_loopback_send(&pair, &server, bad_svc, sizeof(bad_svc)), OSP_ERR_UNSUPPORTED);
	assert_int_equal(pair.client_rx.len, 0);
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
static osp_ic_data_t g_error_data_obj;

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_server, data, len);
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

	osp_ic_data_init(&g_error_data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	g_error_data_obj.value = osp_val_u32(1);
	osp_server_register(&server, osp_ic_data_class(), &g_error_data_obj);

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

static osp_err_t fail_open(void *ctx) {
	(void)ctx;
	return OSP_ERR_NOMEM;
}

static void test_client_open_failure(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);
	pair.client_transport.open = fail_open;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_NOMEM);
}

static void test_client_aare_rejected(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);

	pair.client_transport.send = loopback_send;
	pair.client_transport.recv = loopback_recv;
	pair.client_transport.ctx = &pair;
	g_server = &server;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_MD5, NULL);
	osp_client_set_security(&client, &sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_SECURITY);
	g_server = NULL;
}

static int g_bad_aare_recv_calls;

static osp_err_t bad_aare_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	if (g_bad_aare_recv_calls++ == 0) {
		(void)timeout;
		if (size < 2) {
			return OSP_ERR_NOMEM;
		}
		buf[0] = 0xFF;
		buf[1] = 0xFF;
		*out_len = 2;
		return OSP_OK;
	}
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void test_client_bad_aare_response(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	pair.client_transport.send = loopback_send;
	pair.client_transport.recv = bad_aare_recv;
	pair.client_transport.ctx = &pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	g_server = &server;
	g_bad_aare_recv_calls = 0;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_INVALID);
	g_server = NULL;
	g_bad_aare_recv_calls = 0;
}

static void test_security_hls_errors(void **state) {
	(void)state;
	mock_crypto_init();

	osp_sec_context_t ctx;
	osp_sec_context_init(&ctx, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	ctx.stoc_len = 8;
	memset(ctx.stoc, 0x55, 8);

	uint8_t out[17];
	uint32_t out_len = 0;
	assert_int_equal(osp_hls_pass3_build(NULL, out, sizeof(out), &out_len), -1);
	assert_int_equal(osp_hls_pass3_build(&ctx, out, 16, &out_len), -1);
	assert_int_equal(osp_hls_pass3_build(&ctx, out, sizeof(out), &out_len), 0);
	assert_int_equal(out_len, 17);

	uint8_t tag[OSP_SEC_TAG_SIZE];
	assert_int_equal(osp_hls_gmac(&ctx, ctx.system_title, 1, ctx.stoc, ctx.stoc_len, tag), 0);

	void (*saved_gcm)(osp_sec_key_id, uint32_t, const uint8_t *, uint32_t, const uint8_t *, uint32_t) = osp_hal_gcm_init;
	osp_hal_gcm_init = NULL;
	assert_int_equal(osp_hls_gmac(&ctx, ctx.system_title, 1, ctx.stoc, ctx.stoc_len, tag), -1);
	osp_hal_gcm_init = saved_gcm;

	uint8_t bad_f[17];
	memcpy(bad_f, out, sizeof(out));
	bad_f[16] ^= 0xFF;
	ctx.hls_failures = 0;
	ctx.ic_valid = false;
	assert_int_equal(osp_hls_pass3_verify(&ctx, bad_f, 17), -5);

	uint8_t replay[17];
	memcpy(replay, out, sizeof(out));
	ctx.ic_valid = true;
	ctx.last_peer_ic = ((uint32_t)replay[1] << 24) | ((uint32_t)replay[2] << 16) | ((uint32_t)replay[3] << 8) | (uint32_t)replay[4];
	ctx.hls_failures = 0;
	assert_int_equal(osp_hls_pass3_verify(&ctx, replay, 17), -3);

	ctx.hls_failures = 5;
	assert_int_equal(osp_hls_pass3_verify(&ctx, out, 17), -2);

	osp_sec_context_init(NULL, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
}

static void setup_connected_lowest_client(osp_client_t *client, mock_transport_pair_t *pair, osp_server_t *server) {
	mock_crypto_init();
	mock_transport_pair_init(pair);

	osp_server_init(server, &pair->server_transport, OSP_FRAMING_NONE);
	osp_ic_data_init(&g_error_data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	g_error_data_obj.value = osp_val_u32(1);
	osp_server_register(server, osp_ic_data_class(), &g_error_data_obj);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(server, &sec);

	pair->client_transport.send = loopback_send;
	pair->client_transport.recv = loopback_recv;
	pair->client_transport.ctx = pair;
	g_server = server;

	osp_client_init(client, &pair->client_transport, OSP_FRAMING_NONE);
	osp_client_set_security(client, &sec);
	assert_int_equal(osp_client_connect(client, 5000), OSP_OK);
}

static bool g_corrupt_next_recv;

static osp_err_t corrupt_next_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	if (g_corrupt_next_recv) {
		g_corrupt_next_recv = false;
		(void)timeout;
		buf[0] = 0x00;
		*out_len = 1;
		return OSP_OK;
	}
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void test_client_get_decode_error(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_connected_lowest_client(&client, &pair, &server);
	g_corrupt_next_recv = true;
	pair.client_transport.recv = corrupt_next_recv;

	osp_value_t result;
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	assert_int_equal(osp_client_get(&client, 1, &obis, 2, &result), OSP_ERR_INVALID);
	g_server = NULL;
	g_corrupt_next_recv = false;
}

static void setup_hls_loopback(mock_transport_pair_t *pair, osp_server_t *server, osp_client_t *client) {
	mock_crypto_init();
	mock_transport_pair_init(pair);

	osp_server_init(server, &pair->server_transport, OSP_FRAMING_NONE);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);
	osp_server_set_security(server, &sec);

	pair->client_transport.send = loopback_send;
	pair->client_transport.recv = loopback_recv;
	pair->client_transport.ctx = pair;
	g_server = server;

	osp_client_init(client, &pair->client_transport, OSP_FRAMING_NONE);
	osp_client_set_security(client, &sec);
}

static int g_hls_send_n;

static osp_err_t fail_hls_pass3_send(void *ctx, const uint8_t *data, uint32_t len) {
	if (++g_hls_send_n >= 2) {
		(void)ctx;
		(void)data;
		(void)len;
		return OSP_ERR_NOMEM;
	}
	return loopback_send(ctx, data, len);
}

static void test_client_hls_pass3_send_fail(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_hls_loopback(&pair, &server, &client);
	g_hls_send_n = 0;
	pair.client_transport.send = fail_hls_pass3_send;

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_NOMEM);
	g_server = NULL;
	g_hls_send_n = 0;
}

static int g_hls_recv_n;

static osp_err_t corrupt_hls_pass4_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	osp_err_t r = mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
	if (r == OSP_OK && ++g_hls_recv_n == 2 && *out_len > 5) {
		buf[*out_len - 1] ^= 0xFF;
	}
	return r;
}

static void test_client_hls_bad_pass4(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_hls_loopback(&pair, &server, &client);
	g_hls_recv_n = 0;
	pair.client_transport.recv = corrupt_hls_pass4_recv;

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_SECURITY);
	assert_false(client.associated);
	g_server = NULL;
	g_hls_recv_n = 0;
}

static osp_err_t garbage_hls_pass4_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	if (++g_hls_recv_n >= 2) {
		(void)timeout;
		buf[0] = 0xFF;
		*out_len = 1;
		return OSP_OK;
	}
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void test_client_hls_invalid_pass4_response(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_hls_loopback(&pair, &server, &client);
	g_hls_recv_n = 0;
	pair.client_transport.recv = garbage_hls_pass4_recv;

	assert_int_equal(osp_client_connect(&client, 5000), OSP_ERR_INVALID);
	assert_false(client.associated);
	g_server = NULL;
	g_hls_recv_n = 0;
}

static void test_client_release_bad_rlre(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_connected_lowest_client(&client, &pair, &server);
	g_corrupt_next_recv = true;
	pair.client_transport.recv = corrupt_next_recv;

	assert_int_equal(osp_client_release(&client), OSP_ERR_INVALID);
	assert_true(client.associated);
	g_server = NULL;
	g_corrupt_next_recv = false;
}

static void test_client_set_not_found(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_connected_lowest_client(&client, &pair, &server);

	osp_value_t val = osp_val_u32(9);
	osp_obis_t missing = {9, 9, 9, 9, 9, 9};
	assert_int_equal(osp_client_set(&client, 1, &missing, 2, &val), OSP_ERR_NOT_FOUND);
	g_server = NULL;
}

static void test_client_get_send_failure(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_connected_lowest_client(&client, &pair, &server);
	pair.client_transport.send = fail_send;

	osp_value_t result;
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	assert_int_equal(osp_client_get(&client, 1, &obis, 2, &result), OSP_ERR_NOMEM);
	g_server = NULL;
}

static void test_transport_wrapper_framing(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	const uint8_t apdu[] = {0x60, 0x0A, 0x80, 0x02, 0x07, 0x80, 0xA1, 0x09, 0x06, 0x07};
	uint8_t rx[64];
	uint32_t rx_len;

	assert_int_equal(osp_transport_send_apdu(&pair.client_transport, OSP_FRAMING_WRAPPER, apdu, sizeof(apdu)), OSP_OK);
	assert_int_equal(osp_transport_recv_apdu(&pair.server_transport, OSP_FRAMING_WRAPPER, rx, sizeof(rx), &rx_len, 100), OSP_OK);
	assert_int_equal(rx_len, sizeof(apdu));
	assert_memory_equal(rx, apdu, sizeof(apdu));
}

static void test_hdlc_multibyte_address(void **state) {
	(void)state;
	osp_hdlc_address_t addr;
	uint8_t out[128];
	uint32_t out_len;
	osp_hdlc_frame_t frame, decoded;

	osp_hdlc_address_init(&addr, 0x1234, 2);
	assert_int_equal(addr.length, 2);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x1234);

	osp_hdlc_address_init(&addr, 1, 1);
	assert_int_equal(addr.length, 1);
	assert_int_equal(osp_hdlc_address_value(&addr), 1);

	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 0x1234, 2);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.poll_final = true;
	frame.info[0] = 0xAA;
	frame.info_len = 1;
	assert_int_equal(osp_hdlc_frame(&frame, out, sizeof(out), &out_len), OSP_OK);
	assert_int_equal(osp_hdlc_deframe(out, out_len, &decoded), OSP_OK);
	assert_int_equal(decoded.destination.length, 2);
	assert_int_equal(osp_hdlc_address_value(&decoded.destination), 0x1234);
	assert_int_equal(osp_hdlc_address_value(&decoded.source), 1);
	assert_int_equal(decoded.info_len, 1);
	assert_int_equal(decoded.info[0], 0xAA);
}

static void test_transport_recv_buffer_too_small(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	const uint8_t apdu[] = {0xC0, 0x01, 0x41, 0x00, 0x03, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x02, 0x00};
	uint8_t tiny[4];
	uint32_t rx_len;

	assert_int_equal(osp_transport_send_apdu(&pair.client_transport, OSP_FRAMING_HDLC, apdu, sizeof(apdu)), OSP_OK);
	assert_int_equal(osp_transport_recv_apdu(&pair.server_transport, OSP_FRAMING_HDLC, tiny, sizeof(tiny), &rx_len, 100), OSP_ERR_NOMEM);

	assert_int_equal(osp_transport_send_apdu(&pair.client_transport, OSP_FRAMING_WRAPPER, apdu, sizeof(apdu)), OSP_OK);
	assert_int_equal(osp_transport_recv_apdu(&pair.server_transport, OSP_FRAMING_WRAPPER, tiny, sizeof(tiny), &rx_len, 100), OSP_ERR_NOMEM);
}

static void test_client_disconnect_releases(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;

	setup_connected_lowest_client(&client, &pair, &server);
	assert_true(client.associated);
	assert_int_equal(osp_client_disconnect(&client), OSP_OK);
	assert_false(client.associated);
	g_server = NULL;
}

static void test_server_rlrq_release(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	server.associated = true;

	osp_rlrq_t rlrq = {.reason = 0};
	uint8_t mem[16];
	osp_buf_t buf = {mem, sizeof(mem), 0, 0};
	assert_int_equal(osp_rlrq_encode(&rlrq, &buf), 0);
	assert_int_equal(mem[0], OSP_ACSE_RLRQ_TAG);
	mock_send_to_peer(&pair.server_rx, mem, buf.wr);

	assert_int_equal(osp_server_accept(&server, 5000), OSP_OK);
	assert_false(server.associated);

	uint8_t rx[64];
	uint32_t rx_len;
	assert_int_equal(osp_transport_recv_apdu(&pair.client_transport, OSP_FRAMING_NONE, rx, sizeof(rx), &rx_len, 100), OSP_OK);
	assert_true(rx_len >= 3);
	assert_int_equal(rx[0], OSP_ACSE_RLRE_TAG);
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
	assert_int_equal(osp_server_accept(&server, 5000), OSP_OK);
	assert_int_equal(pair.client_rx.msg_count, 1);
	uint8_t exc[8];
	uint32_t exc_len = 0;
	assert_int_equal(mock_recv_from_peer(&pair.client_rx, exc, sizeof(exc), &exc_len, 0), OSP_OK);
	assert_int_equal(exc[0], OSP_TAG_EXCEPTION_RESPONSE);
	assert_int_equal(exc[2], OSP_EXC_SVC_OPERATION_NOT_POSSIBLE);

	/* Associate first */
	server.associated = true;

	/* Unsupported service tag */
	const uint8_t bad_svc[] = {0xD0, 0x01, 0x41};
	mock_send_to_peer(&pair.server_rx, bad_svc, sizeof(bad_svc));
	assert_int_equal(osp_server_accept(&server, 5000), OSP_ERR_UNSUPPORTED);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security error injection tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_glo_unprotect_wrong_tag(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	/* Build two contexts with DIFFERENT system titles (different IVs) */
	osp_sec_context_t tx, rx;
	uint8_t ek[16], ak[16];
	uint32_t len;

	uint8_t st_tx[] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E};
	uint8_t st_rx[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	hex_to_bytes("000102030405060708090A0B0C0D0E0F", ek, &len);
	hex_to_bytes("D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF", ak, &len);

	osp_sec_context_init(&tx, OSP_SUITE_0, OSP_MECH_LOWEST, st_tx);
	tx.policy = OSP_POLICY_ENCR_AUTH;
	memcpy(tx.guek, ek, 16);
	memcpy(tx.gak, ak, 16);
	tx.invocation_counter = 1;

	osp_sec_context_init(&rx, OSP_SUITE_0, OSP_MECH_LOWEST, st_rx);
	rx.policy = OSP_POLICY_ENCR_AUTH;
	memcpy(rx.guek, ek, 16);
	memcpy(rx.gak, ak, 16);

	/* Encrypt with tx context (system title A) */
	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t ciphered[128];
	uint32_t ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, OSP_GLO_GET_REQUEST, plain, sizeof(plain), ciphered, &ciphered_len), 0);

	/* Try to unprotect with rx context (different system title) -> must fail */
	uint8_t recovered[128];
	uint32_t recovered_len = 0;
	assert_int_not_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
}

static void test_glo_unprotect_replay_ic(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	osp_sec_context_t tx, rx;
	uint8_t ek[16], ak[16], st[8];
	uint32_t len;

	hex_to_bytes("000102030405060708090A0B0C0D0E0F", ek, &len);
	hex_to_bytes("D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF", ak, &len);
	hex_to_bytes("4D4D4D0000BC614E", st, &len);

	osp_sec_context_init(&tx, OSP_SUITE_0, OSP_MECH_LOWEST, st);
	tx.policy = OSP_POLICY_ENCR_AUTH;
	memcpy(tx.guek, ek, 16);
	memcpy(tx.gak, ak, 16);

	osp_sec_context_init(&rx, OSP_SUITE_0, OSP_MECH_LOWEST, st);
	rx.policy = OSP_POLICY_ENCR_AUTH;
	memcpy(rx.guek, ek, 16);
	memcpy(rx.gak, ak, 16);

	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t ciphered[128];
	uint8_t recovered[128];
	uint32_t recovered_len;

	/* First: establish a valid session with IC=5 */
	tx.invocation_counter = 5;
	uint32_t ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, OSP_GLO_GET_REQUEST, plain, sizeof(plain), ciphered, &ciphered_len), 0);
	recovered_len = sizeof(recovered);
	assert_int_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
	assert_true(rx.ic_valid);
	assert_int_equal(rx.last_peer_ic, 5);

	/* Now: encrypt with IC=3 (lower than last_peer_ic=5) -> must fail replay detection */
	tx.invocation_counter = 3;
	ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, OSP_GLO_GET_REQUEST, plain, sizeof(plain), ciphered, &ciphered_len), 0);
	recovered_len = sizeof(recovered);
	assert_int_not_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
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
	    cmocka_unit_test(test_ber_length_and_octet_string),
	    cmocka_unit_test(test_axdr_octet_string_roundtrip),
	    cmocka_unit_test(test_serialize_access_and_objects),
	    cmocka_unit_test(test_wrapper_errors),
	    cmocka_unit_test(test_hdlc_deframe_errors),
	    cmocka_unit_test(test_transport_apdu_errors),
	    cmocka_unit_test(test_loopback_send_propagates_server_error),
	    cmocka_unit_test(test_transport_hdlc_framing),
	    cmocka_unit_test(test_transport_wrapper_framing),
	    cmocka_unit_test(test_hdlc_multibyte_address),
	    cmocka_unit_test(test_transport_recv_buffer_too_small),
	    cmocka_unit_test(test_client_not_connected),
	    cmocka_unit_test(test_client_get_not_found),
	    cmocka_unit_test(test_client_transport_failure),
	    cmocka_unit_test(test_client_open_failure),
	    cmocka_unit_test(test_client_aare_rejected),
	    cmocka_unit_test(test_client_bad_aare_response),
	    cmocka_unit_test(test_security_hls_errors),
	    cmocka_unit_test(test_client_get_decode_error),
	    cmocka_unit_test(test_client_set_not_found),
	    cmocka_unit_test(test_client_get_send_failure),
	    cmocka_unit_test(test_client_hls_pass3_send_fail),
	    cmocka_unit_test(test_client_hls_bad_pass4),
	    cmocka_unit_test(test_client_hls_invalid_pass4_response),
	    cmocka_unit_test(test_client_release_bad_rlre),
	    cmocka_unit_test(test_client_disconnect_releases),
	    cmocka_unit_test(test_glo_unprotect_wrong_tag),
	    cmocka_unit_test(test_glo_unprotect_replay_ic),
	    cmocka_unit_test(test_server_rlrq_release),
	    cmocka_unit_test(test_server_invalid_and_unsupported),
	    cmocka_unit_test(test_server_init_errors),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
