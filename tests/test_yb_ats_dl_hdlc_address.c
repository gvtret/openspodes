/**
 * test_yb_ats_dl_hdlc_address.c — Yellow Book ATS_DL: HDLC address management
 *
 * Maps to test group HDLC_ADDRESS (DLMS UA 1001-3, ATS_DL V5).
 * Tests address encoding/decoding for various byte lengths, broadcast,
 * extension bit, and multi-byte address in full frames.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/transport/transport.h"

/* ── N1: 3-byte address encode/decode roundtrip ─────────────────────────── */

static void test_addr_3byte_roundtrip(void **state) {
	(void)state;

	osp_hdlc_address_t addr;
	osp_hdlc_address_init(&addr, 0x1FFFFF, 3);
	assert_int_equal(addr.length, 3);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x1FFFFF);

	/* Value 0x10000 (exactly 21 bits) */
	osp_hdlc_address_init(&addr, 0x10000, 0);
	assert_int_equal(addr.length, 3);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x10000);

	/* Value 0x4000 (needs 3 bytes) */
	osp_hdlc_address_init(&addr, 0x4000, 0);
	assert_int_equal(addr.length, 3);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x4000);
}

/* ── N2: 4-byte address encode/decode roundtrip ─────────────────────────── */

static void test_addr_4byte_roundtrip(void **state) {
	(void)state;

	osp_hdlc_address_t addr;
	osp_hdlc_address_init(&addr, 0x0FFFFFFF, 4);
	assert_int_equal(addr.length, 4);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x0FFFFFFF);

	osp_hdlc_address_init(&addr, 0x1000000, 4);
	assert_int_equal(addr.length, 4);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x1000000);

	osp_hdlc_address_init(&addr, 0x08000000, 4);
	assert_int_equal(addr.length, 4);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x08000000);
}

/* ── N3: Broadcast address ──────────────────────────────────────────────── */

static void test_addr_broadcast(void **state) {
	(void)state;

	/* DLMS broadcast: typically 0x7F in 1-byte addressing */
	osp_hdlc_address_t addr;
	osp_hdlc_address_init(&addr, 0x7F, 0);
	assert_int_equal(addr.length, 1);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x7F);

	/* 2-byte broadcast: value 0x2000 (needs 2 bytes) */
	osp_hdlc_address_init(&addr, 0x2000, 0);
	assert_int_equal(addr.length, 2);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x2000);
}

/* ── P+: Extension bit in multi-byte addresses ──────────────────────────── */

static void test_addr_extension_bit(void **state) {
	(void)state;

	/* 1-byte address: bit 0 is extension bit, value occupies bits 7:1 */
	osp_hdlc_address_t addr;
	osp_hdlc_address_init(&addr, 1, 0);
	/* value=1, length=1: byte = (1 << 1) | 0x01 = 0x03 (extension bit set) */
	assert_int_equal(addr.bytes[0], 0x03);

	/* 2-byte address: extension bit clear in byte 0, set in byte 1 */
	osp_hdlc_address_init(&addr, 0x3FFD, 0);
	assert_int_equal(addr.length, 2);
	assert_int_equal(addr.bytes[1] & 0x01, 0x01); /* extension bit set in last byte */

	/* 3-byte address: extension bit clear in bytes 0-1, set in byte 2 */
	osp_hdlc_address_init(&addr, 0x10000, 0);
	assert_int_equal(addr.length, 3);
	assert_int_equal(addr.bytes[2] & 0x01, 0x01); /* extension bit set in last byte */
	assert_int_equal(addr.bytes[0] & 0x01, 0x00); /* extension bit clear in first byte */
}

/* ── P+: Value extraction for all lengths ───────────────────────────────── */

static void test_addr_value_extraction(void **state) {
	(void)state;

	osp_hdlc_address_t addr;

	/* 1-byte: values 0..126 (7-bit address space) */
	for (uint32_t v = 0; v <= 126; v++) {
		osp_hdlc_address_init(&addr, v, 0);
		assert_int_equal(osp_hdlc_address_value(&addr), v);
	}

	/* 2-byte: spot check */
	osp_hdlc_address_init(&addr, 128, 0);
	assert_int_equal(osp_hdlc_address_value(&addr), 128);
	osp_hdlc_address_init(&addr, 0x3FFF, 0);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x3FFF);

	/* 3-byte: spot check */
	osp_hdlc_address_init(&addr, 0x40000, 0);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x40000);

	/* 4-byte: spot check (max 28-bit address = 0x0FFFFFFF) */
	osp_hdlc_address_init(&addr, 0x1000000, 0);
	assert_int_equal(osp_hdlc_address_value(&addr), 0x1000000);
}

/* ── P+: Multi-byte address in complete HDLC frame ──────────────────────── */

static void test_addr_in_full_frame(void **state) {
	(void)state;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));

	/* 2-byte destination, 1-byte source */
	osp_hdlc_address_init(&frame.destination, 0x3FFD, 0);
	osp_hdlc_address_init(&frame.source, 1, 0);
	frame.control.type = OSP_HDLC_TYPE_SNRM;
	frame.control.poll_final = true;
	frame.info_len = 0;

	uint8_t encoded[128];
	uint32_t encoded_len = 0;
	assert_int_equal(osp_hdlc_frame(&frame, encoded, sizeof(encoded), &encoded_len), OSP_OK);

	osp_hdlc_frame_t decoded;
	assert_int_equal(osp_hdlc_deframe(encoded, encoded_len, &decoded), OSP_OK);
	assert_int_equal(decoded.destination.length, 2);
	assert_int_equal(osp_hdlc_address_value(&decoded.destination), 0x3FFD);
	assert_int_equal(decoded.source.length, 1);
	assert_int_equal(osp_hdlc_address_value(&decoded.source), 1);
	assert_int_equal(decoded.control.type, OSP_HDLC_TYPE_SNRM);
	assert_true(decoded.control.poll_final);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_addr_3byte_roundtrip),
		cmocka_unit_test(test_addr_4byte_roundtrip),
		cmocka_unit_test(test_addr_broadcast),
		cmocka_unit_test(test_addr_extension_bit),
		cmocka_unit_test(test_addr_value_extraction),
		cmocka_unit_test(test_addr_in_full_frame),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
