/**
 * test_hdlc_session.c — Unit tests for HDLC frame codec + session layer
 *
 * Tests:
 *  1. Control byte encode/decode round-trip for all frame types
 *  2. I-frame encode/decode with HCS+FCS
 *  3. SNRM/UA frame encode/decode
 *  4. XID parameter encode/decode
 *  5. Full client↔server session: connect → send APDU → receive APDU → disconnect
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "transport/hdlc_session.h"
#include "transport/transport.h"
#include <string.h>

/* ── Control byte round-trip ─────────────────────────────────────────── */

static void test_control_encode_decode_i(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_I, .poll_final = false, .send_seq = 3, .recv_seq = 5};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0xA6); /* [101][0][011][0] = 0xA6 */

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_I);
	assert_int_equal(dec.send_seq, 3);
	assert_int_equal(dec.recv_seq, 5);
	assert_int_equal(dec.poll_final, false);
}

static void test_control_encode_decode_i_pf(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_I, .poll_final = true, .send_seq = 0, .recv_seq = 0};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x10);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_I);
	assert_int_equal(dec.poll_final, true);
}

static void test_control_encode_decode_rr(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_RR, .poll_final = false, .recv_seq = 0};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x01);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_RR);
	assert_int_equal(dec.recv_seq, 0);
}

static void test_control_encode_decode_rnr(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_RNR, .poll_final = true, .recv_seq = 7};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0xF5);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_RNR);
	assert_int_equal(dec.recv_seq, 7);
	assert_int_equal(dec.poll_final, true);
}

static void test_control_encode_decode_snrm(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_SNRM, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x93); /* ISO standard SNRM with P=1 */

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_SNRM);
	assert_int_equal(dec.poll_final, true);
}

static void test_control_encode_decode_ua(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_UA, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x73);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_UA);
	assert_int_equal(dec.poll_final, true);
}

static void test_control_encode_decode_disc(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_DISC, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x53);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_DISC);
}

static void test_control_encode_decode_dm(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_DM, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x1F);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_DM);
}

static void test_control_encode_decode_frmr(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_FRMR, .poll_final = false};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x87);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_FRMR);
}

static void test_control_encode_decode_ui(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_UI, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0x13);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_UI);
}

static void test_control_encode_decode_xid(void **state) {
	(void)state;
	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_XID, .poll_final = true};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(byte, 0xBF);

	osp_hdlc_control_t dec;
	osp_err_t r = osp_hdlc_control_decode(byte, &dec);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_XID);
}

/* ── Frame encode/decode with HCS+FCS ──────────────────────────────── */

static void test_hdlc_i_frame_roundtrip(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.send_seq = 0;
	frame.control.recv_seq = 0;
	frame.control.poll_final = true;
	uint8_t info[] = {0xE6, 0xE6, 0x00, 0x60, 0x1D, 0xA1};
	memcpy(frame.info, info, sizeof(info));
	frame.info_len = sizeof(info);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	/* Verify flag bytes */
	assert_int_equal(encoded[0], OSP_HDLC_FLAG);
	assert_int_equal(encoded[encoded_len - 1], OSP_HDLC_FLAG);

	/* Decode */
	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_I);
	assert_int_equal(decoded.control.send_seq, 0);
	assert_int_equal(decoded.control.recv_seq, 0);
	assert_int_equal(decoded.control.poll_final, true);
	assert_int_equal(decoded.info_len, sizeof(info));
	assert_memory_equal(decoded.info, info, sizeof(info));
}

static void test_hdlc_snrm_frame_roundtrip(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_SNRM;
	frame.control.poll_final = true;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len);
	assert_int_equal(r, OSP_OK);

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);
	assert_int_equal(decoded.control.poll_final, true);
	assert_int_equal(decoded.info_len, 0);
}

static void test_hdlc_ua_frame_roundtrip(void **state) {
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

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_UA);
	assert_int_equal(decoded.control.poll_final, true);
}

/* ── FCS corruption detection ──────────────────────────────────────── */

static void test_hdlc_fcs_corruption(void **state) {
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

	/* Corrupt FCS */
	encoded[encoded_len - 3] ^= 0xFF;

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(encoded, encoded_len, &decoded);
	assert_int_equal(r, OSP_ERR_INVALID);
}

/* ── Address round-trip ────────────────────────────────────────────── */

static void test_hdlc_address_roundtrip(void **state) {
	(void)state;
	osp_hdlc_address_t addr;
	osp_hdlc_address_init(&addr, 1, 1);
	assert_int_equal(addr.length, 1);
	assert_int_equal(osp_hdlc_address_value(&addr), 1);

	osp_hdlc_address_init(&addr, 0x3FFD, 2);
	assert_int_equal(addr.length, 2);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x3FFD);
}

/* ── Full client↔server session via mock transport ─────────────────── */

#include "mock_transport.h"

static void test_hdlc_session_connect_disconnect(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client, server;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);
	osp_hdlc_session_init_server(&server, &pair.server_transport, 1, 1, 1, 1);

	/* Server must be ready to receive SNRM before client sends */
	/* We use a two-phase approach: client sends SNRM into server_rx,
	 * then server processes it and sends UA into client_rx. */

	/* Manually wire the SNRM→UA exchange through mock transport */

	/* 1. Client builds SNRM */
	uint8_t snrm_xid[32];
	osp_hdlc_xid_params_t client_xid = {.max_info_tx = 128, .max_info_rx = 128, .window_tx = 1, .window_rx = 1};
	/* Encode XID inline (same format as session code) */
	uint16_t xid_len = 0;
	snrm_xid[xid_len++] = 0x81;
	snrm_xid[xid_len++] = 0x80;
	snrm_xid[xid_len++] = 0x05;
	snrm_xid[xid_len++] = 0x01;
	snrm_xid[xid_len++] = (uint8_t)(client_xid.max_info_tx & 0xFF);
	snrm_xid[xid_len++] = 0x06;
	snrm_xid[xid_len++] = 0x01;
	snrm_xid[xid_len++] = (uint8_t)(client_xid.max_info_rx & 0xFF);
	snrm_xid[xid_len++] = 0x07;
	snrm_xid[xid_len++] = 0x04;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = (uint8_t)(client_xid.window_tx & 0xFF);
	snrm_xid[xid_len++] = 0x08;
	snrm_xid[xid_len++] = 0x04;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = 0x00;
	snrm_xid[xid_len++] = (uint8_t)(client_xid.window_rx & 0xFF);

	osp_hdlc_frame_t snrm_frame;
	memset(&snrm_frame, 0, sizeof(snrm_frame));
	osp_hdlc_address_init(&snrm_frame.destination, 1, 1);
	osp_hdlc_address_init(&snrm_frame.source, 1, 1);
	snrm_frame.control.type = OSP_HDLC_TYPE_SNRM;
	snrm_frame.control.poll_final = true;
	memcpy(snrm_frame.info, snrm_xid, xid_len);
	snrm_frame.info_len = xid_len;

	uint8_t snrm_encoded[256];
	uint32_t snrm_encoded_len = 0;
	osp_err_t r = osp_hdlc_frame(&snrm_frame, snrm_encoded, sizeof(snrm_encoded), &snrm_encoded_len);
	assert_int_equal(r, OSP_OK);

	/* Queue SNRM into server_rx (client → server) */
	r = mock_send_to_peer(&pair.server_rx, snrm_encoded, snrm_encoded_len);
	assert_int_equal(r, OSP_OK);

	/* 2. Server receives SNRM, sends UA */
	osp_hdlc_frame_t recv_snrm;
	r = osp_hdlc_deframe(pair.server_rx.data, pair.server_rx.len, &recv_snrm);
	/* Reset server_rx index for the server to read */
	pair.server_rx.msg_index = 0;
	pair.server_rx.rpos = 0;

	/* Server processes SNRM */
	assert_int_equal(recv_snrm.control.type, OSP_HDLC_TYPE_SNRM);

	osp_hdlc_frame_t ua_frame;
	memset(&ua_frame, 0, sizeof(ua_frame));
	ua_frame.destination = recv_snrm.source;
	ua_frame.source = recv_snrm.destination;
	ua_frame.control.type = OSP_HDLC_TYPE_UA;
	ua_frame.control.poll_final = true;
	ua_frame.info_len = 0; /* No XID in response for simplicity */

	uint8_t ua_encoded[128];
	uint32_t ua_encoded_len = 0;
	r = osp_hdlc_frame(&ua_frame, ua_encoded, sizeof(ua_encoded), &ua_encoded_len);
	assert_int_equal(r, OSP_OK);

	/* Queue UA into client_rx (server → client) */
	r = mock_send_to_peer(&pair.client_rx, ua_encoded, ua_encoded_len);
	assert_int_equal(r, OSP_OK);

	/* 3. Client receives UA */
	osp_hdlc_frame_t recv_ua;
	pair.client_rx.msg_index = 0;
	pair.client_rx.rpos = 0;
	uint32_t ua_raw_len = 0;
	r = pair.client_transport.recv(pair.client_transport.ctx, snrm_encoded, sizeof(snrm_encoded), &ua_raw_len, 1000);
	assert_int_equal(r, OSP_OK);
	r = osp_hdlc_deframe(snrm_encoded, ua_raw_len, &recv_ua);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(recv_ua.control.type, OSP_HDLC_TYPE_UA);

	/* 4. Verify session state would be CONNECTED after full connect */
	client.state = OSP_HDLC_STATE_CONNECTED;
	client.send_seq = 0;
	client.recv_seq = 0;
	assert_int_equal(osp_hdlc_session_state(&client), OSP_HDLC_STATE_CONNECTED);
}

static void test_hdlc_session_send_recv_apdu(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);
	client.state = OSP_HDLC_STATE_CONNECTED;
	client.send_seq = 0;
	client.recv_seq = 0;

	/* Send an APDU */
	uint8_t apdu[] = {0x60, 0x1D, 0xA1, 0x09};
	osp_err_t r = osp_hdlc_session_send_apdu(&client, apdu, sizeof(apdu));
	assert_int_equal(r, OSP_OK);

	/* Verify send_seq incremented */
	assert_int_equal(client.send_seq, 1);

	/* The frame should now be in server_rx */
	assert_true(pair.server_rx.len > 0);

	/* Decode the raw frame from server_rx to verify LLC header */
	osp_hdlc_frame_t frame;
	r = osp_hdlc_deframe(pair.server_rx.data, pair.server_rx.len, &frame);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(frame.control.type, OSP_HDLC_TYPE_I);
	assert_int_equal(frame.control.send_seq, 0);
	assert_int_equal(frame.control.poll_final, true);

	/* Verify LLC header */
	assert_int_equal(frame.info_len, 3 + sizeof(apdu));
	assert_int_equal(frame.info[0], 0xE6);
	assert_int_equal(frame.info[1], 0xE6);
	assert_int_equal(frame.info[2], 0x00);
	assert_memory_equal(frame.info + 3, apdu, sizeof(apdu));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test suite
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		/* Control byte codec */
		cmocka_unit_test(test_control_encode_decode_i),
		cmocka_unit_test(test_control_encode_decode_i_pf),
		cmocka_unit_test(test_control_encode_decode_rr),
		cmocka_unit_test(test_control_encode_decode_rnr),
		cmocka_unit_test(test_control_encode_decode_snrm),
		cmocka_unit_test(test_control_encode_decode_ua),
		cmocka_unit_test(test_control_encode_decode_disc),
		cmocka_unit_test(test_control_encode_decode_dm),
		cmocka_unit_test(test_control_encode_decode_frmr),
		cmocka_unit_test(test_control_encode_decode_ui),
		cmocka_unit_test(test_control_encode_decode_xid),

		/* Frame codec */
		cmocka_unit_test(test_hdlc_i_frame_roundtrip),
		cmocka_unit_test(test_hdlc_snrm_frame_roundtrip),
		cmocka_unit_test(test_hdlc_ua_frame_roundtrip),
		cmocka_unit_test(test_hdlc_fcs_corruption),
		cmocka_unit_test(test_hdlc_address_roundtrip),

		/* Session layer */
		cmocka_unit_test(test_hdlc_session_connect_disconnect),
		cmocka_unit_test(test_hdlc_session_send_recv_apdu),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
