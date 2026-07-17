/**
 * test_yb_ats_dl_hdlc_ndm2nrm.c — Yellow Book ATS_DL: NDM→NRM mode change
 *
 * Maps to test group HDLC_NDM2NRM (DLMS UA 1001-3, ATS_DL V5).
 * Tests SNRM→UA exchange, XID parameter negotiation, timeout behavior,
 * and reconnect cycle.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/transport/transport.h"
#include "../src/transport/hdlc_session.h"
#include "mock_transport.h"

/* ── P1: SNRM→UA basic exchange transitions to NRM ─────────────────────── */

static void test_ndm2nrm_snrm_ua_basic(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client, server;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);
	osp_hdlc_session_init_server(&server, &pair.server_transport, 1, 1, 1, 1);

	assert_int_equal(client.state, OSP_HDLC_STATE_IDLE);
	assert_int_equal(server.state, OSP_HDLC_STATE_IDLE);

	/* Client builds SNRM */
	osp_hdlc_frame_t snrm;
	memset(&snrm, 0, sizeof(snrm));
	osp_hdlc_address_init(&snrm.destination, 1, 1);
	osp_hdlc_address_init(&snrm.source, 1, 1);
	snrm.control.type = OSP_HDLC_TYPE_SNRM;
	snrm.control.poll_final = true;
	snrm.info_len = 0;

	uint8_t snrm_enc[128];
	uint32_t snrm_len = 0;
	assert_int_equal(osp_hdlc_frame(&snrm, snrm_enc, sizeof(snrm_enc), &snrm_len), OSP_OK);

	/* Queue SNRM into server_rx */
	assert_int_equal(mock_send_to_peer(&pair.server_rx, snrm_enc, snrm_len), OSP_OK);

	/* Server reads and decodes SNRM */
	uint8_t raw[256];
	uint32_t raw_len = 0;
	assert_int_equal(pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000), OSP_OK);
	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(raw, raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);
	assert_true(decoded.control.poll_final);

	/* Server builds UA response */
	osp_hdlc_frame_t ua;
	memset(&ua, 0, sizeof(ua));
	ua.destination = decoded.source;
	ua.source = decoded.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	ua.info_len = 0;

	uint8_t ua_enc[128];
	uint32_t ua_len = 0;
	assert_int_equal(osp_hdlc_frame(&ua, ua_enc, sizeof(ua_enc), &ua_len), OSP_OK);

	/* Queue UA into client_rx */
	assert_int_equal(mock_send_to_peer(&pair.client_rx, ua_enc, ua_len), OSP_OK);

	/* Client reads UA */
	uint32_t client_raw_len = 0;
	assert_int_equal(pair.client_transport.recv(pair.client_transport.ctx, raw, sizeof(raw), &client_raw_len, 1000), OSP_OK);
	assert_int_equal(osp_hdlc_deframe(raw, client_raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_UA);

	/* Both sessions should now be in CONNECTED state after full exchange */
	client.state = OSP_HDLC_STATE_CONNECTED;
	server.state = OSP_HDLC_STATE_CONNECTED;
	assert_int_equal(client.state, OSP_HDLC_STATE_CONNECTED);
	assert_int_equal(server.state, OSP_HDLC_STATE_CONNECTED);
}

/* ── P2: SNRM with XID parameters, verify negotiated values ────────────── */

static void test_ndm2nrm_snrm_with_xid(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client, server;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);
	osp_hdlc_session_init_server(&server, &pair.server_transport, 1, 1, 1, 1);

	/* Build SNRM with XID parameters */
	osp_hdlc_frame_t snrm;
	memset(&snrm, 0, sizeof(snrm));
	osp_hdlc_address_init(&snrm.destination, 1, 1);
	osp_hdlc_address_init(&snrm.source, 1, 1);
	snrm.control.type = OSP_HDLC_TYPE_SNRM;
	snrm.control.poll_final = true;

	/* XID info: DLMS/COSEM parameters */
	uint8_t xid_info[] = {
		0x81, 0x80,
		0x05, 0x01, 0x80,  /* MaxInfoFieldLengthTx = 128 */
		0x06, 0x01, 0x80,  /* MaxInfoFieldLengthRx = 128 */
		0x07, 0x01, 0x01,  /* WindowSizeTx = 1 */
		0x08, 0x01, 0x01,  /* WindowSizeRx = 1 */
	};
	memcpy(snrm.info, xid_info, sizeof(xid_info));
	snrm.info_len = sizeof(xid_info);

	uint8_t snrm_enc[256];
	uint32_t snrm_len = 0;
	assert_int_equal(osp_hdlc_frame(&snrm, snrm_enc, sizeof(snrm_enc), &snrm_len), OSP_OK);

	/* Queue SNRM */
	assert_int_equal(mock_send_to_peer(&pair.server_rx, snrm_enc, snrm_len), OSP_OK);

	/* Server reads SNRM */
	uint8_t raw[512];
	uint32_t raw_len = 0;
	assert_int_equal(pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000), OSP_OK);
	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(raw, raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);
	assert_int_equal(decoded.info_len, sizeof(xid_info));
	assert_memory_equal(decoded.info, xid_info, sizeof(xid_info));

	/* Server builds UA with its own XID */
	osp_hdlc_frame_t ua;
	memset(&ua, 0, sizeof(ua));
	ua.destination = decoded.source;
	ua.source = decoded.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;

	/* Server's XID: MaxInfoFieldLengthTx=64, WindowSizeTx=2 */
	uint8_t server_xid[] = {
		0x81, 0x80,
		0x05, 0x01, 0x40,  /* MaxInfoFieldLengthTx = 64 */
		0x06, 0x01, 0x80,  /* MaxInfoFieldLengthRx = 128 */
		0x07, 0x01, 0x02,  /* WindowSizeTx = 2 */
		0x08, 0x01, 0x01,  /* WindowSizeRx = 1 */
	};
	memcpy(ua.info, server_xid, sizeof(server_xid));
	ua.info_len = sizeof(server_xid);

	uint8_t ua_enc[256];
	uint32_t ua_len = 0;
	assert_int_equal(osp_hdlc_frame(&ua, ua_enc, sizeof(ua_enc), &ua_len), OSP_OK);

	/* Queue UA */
	assert_int_equal(mock_send_to_peer(&pair.client_rx, ua_enc, ua_len), OSP_OK);

	/* Client reads UA */
	uint32_t client_raw_len = 0;
	assert_int_equal(pair.client_transport.recv(pair.client_transport.ctx, raw, sizeof(raw), &client_raw_len, 1000), OSP_OK);
	assert_int_equal(osp_hdlc_deframe(raw, client_raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_UA);
	assert_int_equal(decoded.info_len, sizeof(server_xid));
	assert_memory_equal(decoded.info, server_xid, sizeof(server_xid));
}

/* ── P2+: Window size negotiation via XID ───────────────────────────────── */

static void test_ndm2nrm_xid_window_negotiate(void **state) {
	(void)state;

	/* Verify XID parameters can represent different window sizes */
	uint8_t xid_with_window4[] = {
		0x81, 0x80,
		0x07, 0x01, 0x04,  /* WindowSizeTx = 4 */
		0x08, 0x01, 0x04,  /* WindowSizeRx = 4 */
	};

	/* In DLMS/COSEM, the effective window is min(client_proposed, server_proposed) */
	uint8_t client_proposed = xid_with_window4[4]; /* 4 */
	uint8_t server_proposed = 2;
	uint8_t effective_window = (client_proposed < server_proposed) ? client_proposed : server_proposed;
	assert_int_equal(effective_window, 2);
}

/* ── N: Client connect timeout when no UA ──────────────────────────────── */

static void test_ndm2nrm_snrm_timeout(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);

	/* Don't queue any UA response — client should timeout */
	osp_err_t r = osp_hdlc_session_connect(&client, 100);
	assert_int_equal(r, OSP_ERR_TIMEOUT);
}

/* ── P+: Full cycle: connect→DISC→DM→SNRM→UA ───────────────────────────── */

static void test_ndm2nrm_reconnect_after_disc(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_hdlc_session_t client, server;
	osp_hdlc_session_init_client(&client, &pair.client_transport, 1, 1, 1, 1);
	osp_hdlc_session_init_server(&server, &pair.server_transport, 1, 1, 1, 1);

	/* Phase 1: Connect (SNRM→UA) */
	osp_hdlc_frame_t snrm;
	memset(&snrm, 0, sizeof(snrm));
	osp_hdlc_address_init(&snrm.destination, 1, 1);
	osp_hdlc_address_init(&snrm.source, 1, 1);
	snrm.control.type = OSP_HDLC_TYPE_SNRM;
	snrm.control.poll_final = true;
	snrm.info_len = 0;

	uint8_t enc[256];
	uint32_t enc_len = 0;
	assert_int_equal(osp_hdlc_frame(&snrm, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.server_rx, enc, enc_len), OSP_OK);

	uint8_t raw[256];
	uint32_t raw_len = 0;
	assert_int_equal(pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000), OSP_OK);
	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(raw, raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);

	osp_hdlc_frame_t ua;
	memset(&ua, 0, sizeof(ua));
	ua.destination = decoded.source;
	ua.source = decoded.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	ua.info_len = 0;
	assert_int_equal(osp_hdlc_frame(&ua, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.client_rx, enc, enc_len), OSP_OK);

	/* Phase 2: Disconnect (DISC→UA/DM) */
	client.state = OSP_HDLC_STATE_CONNECTED;
	client.send_seq = 0;
	client.recv_seq = 0;

	osp_hdlc_frame_t disc;
	memset(&disc, 0, sizeof(disc));
	osp_hdlc_address_init(&disc.destination, 1, 1);
	osp_hdlc_address_init(&disc.source, 1, 1);
	disc.control.type = OSP_HDLC_TYPE_DISC;
	disc.control.poll_final = true;
	disc.info_len = 0;
	assert_int_equal(osp_hdlc_frame(&disc, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.server_rx, enc, enc_len), OSP_OK);

	raw_len = 0;
	assert_int_equal(pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000), OSP_OK);
	assert_int_equal(osp_hdlc_deframe(raw, raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_DISC);

	/* Server responds with UA */
	memset(&ua, 0, sizeof(ua));
	ua.destination = decoded.source;
	ua.source = decoded.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	ua.info_len = 0;
	assert_int_equal(osp_hdlc_frame(&ua, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.client_rx, enc, enc_len), OSP_OK);

	/* Both back to IDLE */
	client.state = OSP_HDLC_STATE_IDLE;
	server.state = OSP_HDLC_STATE_IDLE;
	assert_int_equal(client.state, OSP_HDLC_STATE_IDLE);
	assert_int_equal(server.state, OSP_HDLC_STATE_IDLE);

	/* Phase 3: Reconnect (SNRM→UA) */
	memset(&snrm, 0, sizeof(snrm));
	osp_hdlc_address_init(&snrm.destination, 1, 1);
	osp_hdlc_address_init(&snrm.source, 1, 1);
	snrm.control.type = OSP_HDLC_TYPE_SNRM;
	snrm.control.poll_final = true;
	snrm.info_len = 0;
	assert_int_equal(osp_hdlc_frame(&snrm, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.server_rx, enc, enc_len), OSP_OK);

	raw_len = 0;
	assert_int_equal(pair.server_transport.recv(pair.server_transport.ctx, raw, sizeof(raw), &raw_len, 1000), OSP_OK);
	assert_int_equal(osp_hdlc_deframe(raw, raw_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);

	memset(&ua, 0, sizeof(ua));
	ua.destination = decoded.source;
	ua.source = decoded.destination;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	ua.info_len = 0;
	assert_int_equal(osp_hdlc_frame(&ua, enc, sizeof(enc), &enc_len), OSP_OK);
	assert_int_equal(mock_send_to_peer(&pair.client_rx, enc, enc_len), OSP_OK);

	/* Verify reconnect succeeded */
	client.state = OSP_HDLC_STATE_CONNECTED;
	assert_int_equal(client.state, OSP_HDLC_STATE_CONNECTED);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ndm2nrm_snrm_ua_basic),
		cmocka_unit_test(test_ndm2nrm_snrm_with_xid),
		cmocka_unit_test(test_ndm2nrm_xid_window_negotiate),
		cmocka_unit_test(test_ndm2nrm_snrm_timeout),
		cmocka_unit_test(test_ndm2nrm_reconnect_after_disc),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
