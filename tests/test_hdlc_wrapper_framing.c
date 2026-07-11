/**
 * test_hdlc_wrapper_framing.c — Integration tests for HDLC and WRAPPER framing
 *
 * Tests the full client↔server flow through actual framing codecs:
 *  1. WRAPPER codec roundtrip
 *  2. HDLC codec roundtrip (I/S/U-frames)
 *  3. HDLC session APDU roundtrip (SNRM/UA + I-frame with LLC)
 *  4. WRAPPER full E2E: AARQ→AARE→HLS→GET/SET/RELEASE
 *  5. HDLC full E2E: SNRM/UA→AARQ→AARE→HLS→GET/RELEASE
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
#include "../src/transport/transport.h"
#include "../src/transport/hdlc_session.h"
#include "../src/security/security.h"
#include "../src/codec/serialize.h"
#include "mock_transport.h"
#include "mock_crypto.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Loopback transport (same pattern as test_integration.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_server_t *g_server = NULL;

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_server, data, len);
}

static osp_err_t loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static void setup_loopback(mock_transport_pair_t *pair, osp_server_t *server) {
	mock_transport_pair_init(pair);
	pair->client_transport.send = loopback_send;
	pair->client_transport.recv = loopback_recv;
	pair->client_transport.ctx = pair;
	g_server = server;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  1. WRAPPER codec roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wrapper_codec_roundtrip(void **state) {
	(void)state;
	uint8_t apdu[] = {0x60, 0x1D, 0xA1, 0x09, 0x06, 0x07, 0x60, 0x85};
	uint8_t out[64];
	uint32_t out_len = 0;

	osp_err_t r = osp_wrapper_encode(0x1234, 0x5678, apdu, sizeof(apdu), out, sizeof(out), &out_len);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(out_len, OSP_WRAPPER_HEADER_SIZE + sizeof(apdu));

	osp_wrapper_header_t hdr;
	const uint8_t *decoded_apdu;
	uint32_t decoded_len;
	r = osp_wrapper_decode(out, out_len, &hdr, &decoded_apdu, &decoded_len);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(hdr.version, 0x0001);
	assert_int_equal(hdr.source, 0x1234);
	assert_int_equal(hdr.destination, 0x5678);
	assert_int_equal(hdr.length, sizeof(apdu));
	assert_memory_equal(decoded_apdu, apdu, sizeof(apdu));
}

static void test_wrapper_decode_short_buffer(void **state) {
	(void)state;
	uint8_t short_buf[4] = {0x00, 0x01, 0x00, 0x08};
	osp_wrapper_header_t hdr;
	const uint8_t *apdu;
	uint32_t apdu_len;
	osp_err_t r = osp_wrapper_decode(short_buf, sizeof(short_buf), &hdr, &apdu, &apdu_len);
	assert_int_equal(r, OSP_ERR_INVALID);
}

static void test_wrapper_decode_wrong_version(void **state) {
	(void)state;
	uint8_t buf[16] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
	osp_wrapper_header_t hdr;
	const uint8_t *apdu;
	uint32_t apdu_len;
	osp_err_t r = osp_wrapper_decode(buf, sizeof(buf), &hdr, &apdu, &apdu_len);
	assert_int_equal(r, OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  2. HDLC codec roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_hdlc_i_frame_with_payload(void **state) {
	(void)state;
	uint8_t payload[] = {0xE6, 0xE6, 0x00, 0x60, 0x1D, 0xA1, 0x09};
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.poll_final = true;
	memcpy(frame.info, payload, sizeof(payload));
	frame.info_len = sizeof(payload);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_I);
	assert_int_equal(decoded.control.poll_final, true);
	assert_int_equal(decoded.info_len, sizeof(payload));
	assert_memory_equal(decoded.info, payload, sizeof(payload));
}

static void test_hdlc_i_frame_seq_numbers(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 3, 1);
	osp_hdlc_address_init(&frame.source, 2, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.send_seq = 5;
	frame.control.recv_seq = 3;
	frame.control.poll_final = false;
	frame.info_len = 0;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.send_seq, 5);
	assert_int_equal(decoded.control.recv_seq, 3);
	assert_int_equal(decoded.control.poll_final, false);
}

static void test_hdlc_disc_dm_exchange(void **state) {
	(void)state;
	osp_hdlc_frame_t disc;
	memset(&disc, 0, sizeof(disc));
	osp_hdlc_address_init(&disc.destination, 1, 1);
	osp_hdlc_address_init(&disc.source, 2, 1);
	disc.control.type = OSP_HDLC_TYPE_DISC;
	disc.control.poll_final = true;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&disc, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_DISC);
	assert_int_equal(decoded.control.poll_final, true);

	osp_hdlc_frame_t dm;
	memset(&dm, 0, sizeof(dm));
	osp_hdlc_address_init(&dm.destination, 2, 1);
	osp_hdlc_address_init(&dm.source, 1, 1);
	dm.control.type = OSP_HDLC_TYPE_DM;
	dm.control.poll_final = true;

	r = osp_hdlc_frame(&dm, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_DM);
}

static void test_hdlc_fcs_corruption_detected(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_UA;
	frame.control.poll_final = true;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	encoded[4] ^= 0xFF; /* corrupt a data byte */
	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  3. HDLC session APDU roundtrip (manual handshake via mock transport)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_hdlc_session_apdu_roundtrip(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client, server;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 2, 1, 3, 1);
	osp_hdlc_session_init_server(&server, &pair.server_transport, 3, 1, 2, 1);

	/* Manual SNRM/UA: client sends SNRM */
	osp_hdlc_frame_t snrm;
	memset(&snrm, 0, sizeof(snrm));
	snrm.destination = server.client_addr;
	snrm.source = client.client_addr;
	snrm.control.type = OSP_HDLC_TYPE_SNRM;
	snrm.control.poll_final = true;
	uint8_t snrm_enc[128];
	uint32_t snrm_len = 0;
	osp_hdlc_frame(&snrm, snrm_enc, sizeof(snrm_enc), &snrm_len);
	mock_send_to_peer(&pair.server_rx, snrm_enc, snrm_len);

	/* Server reads SNRM, sends UA */
	uint8_t raw[256];
	uint32_t raw_len = 0;
	osp_err_t r = pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000);
	assert_int_equal(r, OSP_OK);
	osp_hdlc_frame_t recv_frame;
	r = osp_hdlc_deframe(raw, raw_len, &recv_frame);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(recv_frame.control.type, OSP_HDLC_TYPE_SNRM);

	osp_hdlc_frame_t ua;
	memset(&ua, 0, sizeof(ua));
	ua.destination = recv_frame.source;
	ua.source = recv_frame.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	uint8_t ua_enc[128];
	uint32_t ua_len = 0;
	osp_hdlc_frame(&ua, ua_enc, sizeof(ua_enc), &ua_len);
	mock_send_to_peer(&pair.client_rx, ua_enc, ua_len);

	/* Client reads UA */
	uint32_t client_raw_len = 0;
	r = pair.client_transport.recv(pair.client_transport.ctx, raw, sizeof(raw), &client_raw_len, 1000);
	assert_int_equal(r, OSP_OK);
	r = osp_hdlc_deframe(raw, client_raw_len, &recv_frame);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(recv_frame.control.type, OSP_HDLC_TYPE_UA);

	/* Both sessions connected */
	client.state = OSP_HDLC_STATE_CONNECTED;
	client.send_seq = 0;
	client.recv_seq = 0;
	server.state = OSP_HDLC_STATE_CONNECTED;
	server.send_seq = 0;
	server.recv_seq = 0;

	/* Client sends APDU */
	uint8_t apdu[] = {0x60, 0x1D, 0xA1, 0x09};
	r = osp_hdlc_session_send_apdu(&client, apdu, sizeof(apdu));
	assert_int_equal(r, OSP_OK);

	/* Server receives APDU */
	uint8_t rx_buf[256];
	uint32_t rx_len = 0;
	r = osp_hdlc_session_recv_apdu(&server, rx_buf, sizeof(rx_buf), &rx_len, 1000);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(rx_len, sizeof(apdu));
	assert_memory_equal(rx_buf, apdu, sizeof(apdu));

	/* Server sends response */
	uint8_t resp[] = {0x08, 0x00, 0x06};
	r = osp_hdlc_session_send_apdu(&server, resp, sizeof(resp));
	assert_int_equal(r, OSP_OK);

	/* Client receives response */
	r = osp_hdlc_session_recv_apdu(&client, rx_buf, sizeof(rx_buf), &rx_len, 1000);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(rx_len, sizeof(resp));
	assert_memory_equal(rx_buf, resp, sizeof(resp));

	/* Verify sequence numbers */
	assert_int_equal(client.send_seq, 1);
	assert_int_equal(server.recv_seq, 1);
	assert_int_equal(server.send_seq, 1);
	assert_int_equal(client.recv_seq, 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  4. WRAPPER full E2E: AARQ→AARE→HLS→GET→SET→RELEASE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wrapper_full_e2e(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_WRAPPER);
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(42);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);
	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	setup_loopback(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_WRAPPER);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	/* Connect: AARQ→AARE through wrapper framing (loopback auto-processes server) */
	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	/* GET */
	osp_value_t result;
	osp_obis_t dobis = {0, 0, 1, 0, 0, 255};
	assert_int_equal(osp_client_get(&client, 1, &dobis, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 42);

	/* SET */
	osp_value_t newval = osp_val_u32(100);
	assert_int_equal(osp_client_set(&client, 1, &dobis, 1, &newval), OSP_OK);
	assert_int_equal(data_obj.value.as.uint32.value, 100);

	/* GET back */
	assert_int_equal(osp_client_get(&client, 1, &dobis, 1, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 100);

	/* Release */
	assert_int_equal(osp_client_release(&client), OSP_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  5. HDLC full E2E: SNRM/UA→AARQ→AARE→GET→RELEASE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_hdlc_full_e2e(void **state) {
	(void)state;
	mock_crypto_init();
	mock_transport_pair_t pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_HDLC);
	osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_obj.value = osp_val_u32(77);
	osp_server_register(&server, osp_ic_data_class(), &data_obj);
	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	setup_loopback(&pair, &server);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_HDLC);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	/* Client connect: SNRM/UA → AARQ/AARE (loopback auto-processes server) */
	osp_err_t r = osp_client_connect(&client, 5000);
	assert_int_equal(r, OSP_OK);

	/* GET */
	osp_value_t result;
	osp_obis_t dobis = {0, 0, 1, 0, 0, 255};
	r = osp_client_get(&client, 1, &dobis, 1, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 77);

	/* Release + Disconnect */
	assert_int_equal(osp_client_release(&client), OSP_OK);
	osp_client_disconnect(&client);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test suite
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		/* WRAPPER codec */
		cmocka_unit_test(test_wrapper_codec_roundtrip),
		cmocka_unit_test(test_wrapper_decode_short_buffer),
		cmocka_unit_test(test_wrapper_decode_wrong_version),

		/* HDLC codec */
		cmocka_unit_test(test_hdlc_i_frame_with_payload),
		cmocka_unit_test(test_hdlc_i_frame_seq_numbers),
		cmocka_unit_test(test_hdlc_disc_dm_exchange),
		cmocka_unit_test(test_hdlc_fcs_corruption_detected),

		/* HDLC session */
		cmocka_unit_test(test_hdlc_session_apdu_roundtrip),

		/* Full E2E through framing */
		cmocka_unit_test(test_wrapper_full_e2e),
		cmocka_unit_test(test_hdlc_full_e2e),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
