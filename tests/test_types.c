/**
 * test_types.c — Tests for IEC 62056 typed attributes
 *
 * Tests BITSTRING, UTF8STRING, BCD, context_name, season, week_profile,
 * day_profile serialization roundtrips.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "codec/codec.h"
#include "codec/serialize.h"
#include "codec/structures.h"
#include "ic/ic_common.h"

/* ── BITSTRING roundtrip ────────────────────────────────────────────────── */

static void test_bitstring_roundtrip(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[128];
	osp_buf_init(&buf, tx, sizeof(tx));

	osp_value_t val;
	val.tag = OSP_TAG_BITSTRING;
	val.as.bitstring.bits[0] = 0xA5; /* 10100101 */
	val.as.bitstring.num_bits = 8;

	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);

	buf.rd = 0;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_BITSTRING);
	assert_int_equal(decoded.as.bitstring.num_bits, 8);
	assert_int_equal(decoded.as.bitstring.bits[0], 0xA5);
}

/* ── UTF8STRING roundtrip ───────────────────────────────────────────────── */

static void test_utf8string_roundtrip(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[128];
	osp_buf_init(&buf, tx, sizeof(tx));

	osp_value_t val;
	val.tag = OSP_TAG_UTF8STRING;
	const char *test = "Hello";
	val.as.utf8string.len = 5;
	memcpy(val.as.utf8string.data, test, 5);

	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);

	buf.rd = 0;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_UTF8STRING);
	assert_int_equal(decoded.as.utf8string.len, 5);
	assert_memory_equal(decoded.as.utf8string.data, test, 5);
}

/* ── BCD roundtrip ──────────────────────────────────────────────────────── */

static void test_bcd_roundtrip(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[32];
	osp_buf_init(&buf, tx, sizeof(tx));

	osp_value_t val;
	val.tag = OSP_TAG_BCD;
	val.as.bcd.value = 0x12;

	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);

	buf.rd = 0;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_BCD);
	assert_int_equal(decoded.as.bcd.value, 0x12);
}

/* ── context_name roundtrip ─────────────────────────────────────────────── */

static void test_context_name_structure(void **state) {
	(void)state;
	osp_context_name_t cn;
	cn.is_structure = true;
	cn.as.structure.joint_iso_ctt = 0;
	cn.as.structure.country = 2;
	cn.as.structure.country_name = 16;
	cn.as.structure.identified_organization = 4;
	cn.as.structure.dlms_ua = 254;
	cn.as.structure.application_context = 1;
	cn.as.structure.context_id = 1;

	osp_value_t val = osp_ic_val_context_name(&cn);
	assert_int_equal(val.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(val.as.structure.elements.count, 7);

	osp_context_name_t decoded;
	memset(&decoded, 0, sizeof(decoded));
	assert_int_equal(osp_ic_read_context_name(&val, &decoded), OSP_OK);
	assert_true(decoded.is_structure);
	assert_int_equal(decoded.as.structure.joint_iso_ctt, 0);
	assert_int_equal(decoded.as.structure.context_id, 1);
}

static void test_context_name_oid(void **state) {
	(void)state;
	osp_context_name_t cn;
	cn.is_structure = false;
	cn.as.oid.len = 6;
	memcpy(cn.as.oid.data, (uint8_t[]){0x60, 0x85, 0x74, 0x05, 0x08, 0x01}, 6);

	osp_value_t val = osp_ic_val_context_name(&cn);
	assert_int_equal(val.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(val.as.octetstring.len, 6);

	osp_context_name_t decoded;
	memset(&decoded, 0, sizeof(decoded));
	assert_int_equal(osp_ic_read_context_name(&val, &decoded), OSP_OK);
	assert_false(decoded.is_structure);
	assert_int_equal(decoded.as.oid.len, 6);
	assert_memory_equal(decoded.as.oid.data, cn.as.oid.data, 6);
}

/* ── Season roundtrip ───────────────────────────────────────────────────── */

static void test_season_roundtrip(void **state) {
	(void)state;
	osp_season_t s;
	memset(&s, 0, sizeof(s));
	s.name_len = 6;
	memcpy(s.name, "Winter", 6);
	s.start = (osp_obis_t){0, 0, 10, 0, 1, 0}; /* Simple OBIS */
	s.week_name_len = 4;
	memcpy(s.week_name, "W01", 4);

	osp_value_t val = osp_ic_val_season(&s);
	assert_int_equal(val.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(val.as.structure.elements.count, 3);

	osp_season_t decoded;
	memset(&decoded, 0, sizeof(decoded));
	assert_int_equal(osp_ic_read_season(&val, &decoded), OSP_OK);
	assert_int_equal(decoded.name_len, 6);
	assert_memory_equal(decoded.name, "Winter", 6);
	assert_int_equal(decoded.start.a, 0);
	assert_int_equal(decoded.start.c, 10);
	assert_int_equal(decoded.week_name_len, 4);
}

/* ── Week profile roundtrip ─────────────────────────────────────────────── */

static void test_week_profile_roundtrip(void **state) {
	(void)state;
	osp_week_profile_t wp;
	memset(&wp, 0, sizeof(wp));
	wp.name_len = 5;
	memcpy(wp.name, "Week1", 5);
	wp.day_names[0][0] = 1; /* Monday */
	wp.day_names[1][0] = 1; /* Tuesday */
	wp.day_names[6][0] = 2; /* Sunday */

	osp_value_t val = osp_ic_val_week_profile(&wp);
	assert_int_equal(val.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(val.as.structure.elements.count, 8);

	osp_week_profile_t decoded;
	memset(&decoded, 0, sizeof(decoded));
	assert_int_equal(osp_ic_read_week_profile(&val, &decoded), OSP_OK);
	assert_int_equal(decoded.name_len, 5);
	assert_memory_equal(decoded.name, "Week1", 5);
	assert_int_equal(decoded.day_names[0][0], 1);
	assert_int_equal(decoded.day_names[6][0], 2);
}

/* ── Day profile roundtrip ──────────────────────────────────────────────── */

static void test_day_profile_roundtrip(void **state) {
	(void)state;
	osp_day_profile_t dp;
	memset(&dp, 0, sizeof(dp));
	dp.name_len = 3;
	memcpy(dp.name, "Day", 3);
	dp.action_count = 2;

	osp_value_t val = osp_ic_val_day_profile(&dp);
	assert_int_equal(val.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(val.as.structure.elements.count, 2);

	osp_day_profile_t decoded;
	memset(&decoded, 0, sizeof(decoded));
	assert_int_equal(osp_ic_read_day_profile(&val, &decoded), OSP_OK);
	assert_int_equal(decoded.name_len, 3);
	assert_memory_equal(decoded.name, "Day", 3);
	assert_int_equal(decoded.action_count, 2);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_bitstring_roundtrip),
		cmocka_unit_test(test_utf8string_roundtrip),
		cmocka_unit_test(test_bcd_roundtrip),
		cmocka_unit_test(test_context_name_structure),
		cmocka_unit_test(test_context_name_oid),
		cmocka_unit_test(test_season_roundtrip),
		cmocka_unit_test(test_week_profile_roundtrip),
		cmocka_unit_test(test_day_profile_roundtrip),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
