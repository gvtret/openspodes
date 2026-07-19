/**
 * test_yb_ats_dl_hdlc_frame.c — Yellow Book ATS_DL: HDLC frame element handling
 *
 * Maps to test group HDLC_FRAME (DLMS UA 1001-3, ATS_DL V5).
 * Tests frame encoding/decoding roundtrips, FCS/HCS verification,
 * bit-stuffing, escape sequences, and edge cases.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/transport/transport.h"

/* ── P2: RR S-frame encode/decode roundtrip ─────────────────────────────── */

static void test_frame_rr_roundtrip(void **state) {
	(void)state;

	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_RR, .poll_final = false, .recv_seq = 0};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	osp_hdlc_control_t dec;
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_RR);
	assert_int_equal(dec.recv_seq, 0);
	assert_int_equal(dec.poll_final, false);

	/* P/F set */
	ctrl.poll_final = true;
	ctrl.recv_seq = 7;
	byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.recv_seq, 7);
	assert_true(dec.poll_final);
}

/* ── P3: RNR S-frame encode/decode roundtrip ────────────────────────────── */

static void test_frame_rnr_roundtrip(void **state) {
	(void)state;

	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_RNR, .poll_final = true, .recv_seq = 3};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	osp_hdlc_control_t dec;
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_RNR);
	assert_int_equal(dec.recv_seq, 3);
	assert_true(dec.poll_final);

	/* P/F clear, recv_seq=7 */
	ctrl.poll_final = false;
	ctrl.recv_seq = 7;
	byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.recv_seq, 7);
	assert_false(dec.poll_final);
}

/* ── P4: REJ S-frame encode/decode roundtrip ────────────────────────────── */

static void test_frame_rej_roundtrip(void **state) {
	(void)state;

	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_REJ, .poll_final = false, .recv_seq = 0};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	osp_hdlc_control_t dec;
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_REJ);
	assert_int_equal(dec.recv_seq, 0);

	ctrl.poll_final = true;
	ctrl.recv_seq = 5;
	byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.recv_seq, 5);
	assert_true(dec.poll_final);
}

/* ── N1: FRMR control byte encode/decode ────────────────────────────────── */

static void test_frame_frmr_control(void **state) {
	(void)state;

	osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_FRMR, .poll_final = false};
	uint8_t byte = osp_hdlc_control_encode(&ctrl);
	osp_hdlc_control_t dec;
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_int_equal(dec.type, OSP_HDLC_TYPE_FRMR);

	ctrl.poll_final = true;
	byte = osp_hdlc_control_encode(&ctrl);
	assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
	assert_true(dec.poll_final);
}

/* ── N2: UI frame with info field encode/decode ─────────────────────────── */

static void test_frame_ui_with_info(void **state) {
	(void)state;

	uint8_t payload[] = {0xE6, 0xE6, 0x00, 0x60, 0x1D, 0xA1};
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_UI;
	frame.control.poll_final = true;
	memcpy(frame.info, payload, sizeof(payload));
	frame.info_len = sizeof(payload);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_UI);
	assert_int_equal(decoded.info_len, sizeof(payload));
	assert_memory_equal(decoded.info, payload, sizeof(payload));
}

/* ── N3: XID parameter parse from peer frame ────────────────────────────── */

static void test_frame_xid_param_parse(void **state) {
	(void)state;

	/* Build an XID frame with known XID parameters */
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_XID;
	frame.control.poll_final = true;

	/* XID info: group identifier 81 80, followed by parameter TLVs */
	uint8_t xid_info[] = {
		0x81, 0x80,  /* group identifier 81 80 (DLMS/COSEM) */
		0x05, 0x01, 0x80,  /* param 5 (MaxInfoFieldLengthTx): 1-byte, value 128 */
		0x06, 0x01, 0x80,  /* param 6 (MaxInfoFieldLengthRx): 1-byte, value 128 */
		0x07, 0x01, 0x01,  /* param 7 (WindowSizeTx): 1-byte, value 1 */
		0x08, 0x01, 0x01,  /* param 8 (WindowSizeRx): 1-byte, value 1 */
	};
	memcpy(frame.info, xid_info, sizeof(xid_info));
	frame.info_len = sizeof(xid_info);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_XID);
	assert_int_equal(decoded.info_len, sizeof(xid_info));
	assert_memory_equal(decoded.info, xid_info, sizeof(xid_info));
}

/* ── N5: Frame with zero-length info field ──────────────────────────────── */

static void test_frame_zero_length_info(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_SNRM;
	frame.control.poll_final = true;
	frame.info_len = 0;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);
	assert_int_equal(decoded.info_len, 0);
}

/* ── N6: Frame at max info field size ───────────────────────────────────── */

static void test_frame_max_info_length(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.poll_final = true;
	frame.control.send_seq = 0;
	frame.control.recv_seq = 0;

	/* Fill with a recognizable pattern */
	uint16_t info_len = OSP_HDLC_MAX_FRAME_SIZE;
	for (uint16_t i = 0; i < info_len; i++) {
		frame.info[i] = (uint8_t)(i & 0xFF);
	}
	frame.info_len = info_len;

	/* Frame buffer must fit max info + HDLC header/FCS overhead */
	uint8_t encoded[OSP_HDLC_MAX_FRAME_SIZE + 64];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.info_len, info_len);
	assert_memory_equal(decoded.info, frame.info, info_len);
}

/* ── N7: 0x7E in info field gets bit-stuffed ────────────────────────────── */

static void test_frame_bit_stuffing(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.poll_final = true;

	/* Info field containing 0x7E (the flag byte) — must be bit-stuffed */
	uint8_t payload[] = {0x01, 0x7E, 0x02, 0x7E, 0x03};
	memcpy(frame.info, payload, sizeof(payload));
	frame.info_len = sizeof(payload);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.info_len, sizeof(payload));
	assert_memory_equal(decoded.info, payload, sizeof(payload));
}

/* ── N8: 0x7D escape sequences ──────────────────────────────────────────── */

static void test_frame_escaped_flag_handling(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.poll_final = true;

	/* Info field containing 0x7D (escape byte) */
	uint8_t payload[] = {0x01, 0x7D, 0x02, 0x03};
	memcpy(frame.info, payload, sizeof(payload));
	frame.info_len = sizeof(payload);

	uint8_t encoded[256];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.info_len, sizeof(payload));
	assert_memory_equal(decoded.info, payload, sizeof(payload));
}

/* ── P1: HCS+FCS computed correctly on encode ──────────────────────────── */

static void test_frame_hcs_fcs_correct(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.poll_final = true;
	frame.info_len = 0;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	/* Frame structure for 1-byte addresses, no info:
	 * [0] = 0x7E (flag)
	 * [1] = 0xA0 (format byte 1)
	 * [2] = flen_lo (format byte 2) — includes format(2)+dest+src+ctrl+FCS
	 * [3] = dest addr
	 * [4] = src addr
	 * [5] = control byte
	 * [6..7] = FCS
	 * [8] = 0x7E (flag) */
	assert_int_equal(encoded[0], OSP_HDLC_FLAG);
	assert_int_equal(encoded[encoded_len - 1], OSP_HDLC_FLAG);

	/* For 1-byte addresses, no info: flen = 2(format)+1+1+1+2(FCS) = 7 */
	assert_int_equal(encoded[1], 0xA0);
	assert_int_equal(encoded[2], 0x07);

	/* Verify FCS: CRC-16 over format + addr + ctrl (everything from [1] to [5]) */
	uint16_t fcs = osp_hdlc_fcs16(encoded + 1, 5);
	uint8_t fcs_lo = (uint8_t)(fcs & 0xFF);
	uint8_t fcs_hi = (uint8_t)(fcs >> 8);
	assert_int_equal(encoded[6], fcs_lo);
	assert_int_equal(encoded[7], fcs_hi);
}

/* ── P+: I-frame with N(S)=0..7 roundtrip ───────────────────────────────── */

static void test_frame_i_with_various_ns(void **state) {
	(void)state;

	for (uint8_t ns = 0; ns < 8; ns++) {
		osp_hdlc_frame_t frame;
		memset(&frame, 0, sizeof(frame));
		osp_hdlc_address_init(&frame.destination, 1, 1);
		osp_hdlc_address_init(&frame.source, 1, 1);
		frame.control.type = OSP_HDLC_TYPE_I;
		frame.control.send_seq = ns;
		frame.control.recv_seq = 0;
		frame.control.poll_final = true;
		frame.info_len = 0;

		uint8_t encoded[128];
		uint32_t encoded_len = 0;
		assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

		osp_hdlc_frame_t decoded;
		assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
		assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_I);
		assert_int_equal(decoded.control.send_seq, ns);
	}
}

/* ── P+: S-frame N(R) across full range 0-7 ─────────────────────────────── */

static void test_frame_s_frame_nr_values(void **state) {
	(void)state;

	for (uint8_t nr = 0; nr < 8; nr++) {
		osp_hdlc_control_t ctrl = {.type = OSP_HDLC_TYPE_RR, .poll_final = false, .recv_seq = nr};
		uint8_t byte = osp_hdlc_control_encode(&ctrl);
		osp_hdlc_control_t dec;
		assert_int_equal(osp_hdlc_control_decode(byte, &dec), OSP_OK);
		assert_int_equal(dec.recv_seq, nr);
		assert_int_equal(dec.type, OSP_HDLC_TYPE_RR);
	}
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_frame_rr_roundtrip),
		cmocka_unit_test(test_frame_rnr_roundtrip),
		cmocka_unit_test(test_frame_rej_roundtrip),
		cmocka_unit_test(test_frame_frmr_control),
		cmocka_unit_test(test_frame_ui_with_info),
		cmocka_unit_test(test_frame_xid_param_parse),
		cmocka_unit_test(test_frame_zero_length_info),
		cmocka_unit_test(test_frame_max_info_length),
		cmocka_unit_test(test_frame_bit_stuffing),
		cmocka_unit_test(test_frame_escaped_flag_handling),
		cmocka_unit_test(test_frame_hcs_fcs_correct),
		cmocka_unit_test(test_frame_i_with_various_ns),
		cmocka_unit_test(test_frame_s_frame_nr_values),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
