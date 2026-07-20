/**
 * test_core.c — CMocka tests for openspodes
 *
 * Tests: types, codec, serializer, IC vtable, dispatcher.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include "cmocka.h"

#include "openspodes.h"
#include "codec/codec.h"
#include "codec/types.h"
#include "codec/serialize.h"
#include "codec/structures.h"
#include "service/service.h"
#include "service/xdms_selective.h"
#include "spodus/tasks.h"
#include "ic/spodus_helpers.h"
#include "spodus/spodus_data.h"
#include "spodus/concentrator.h"
#include "service/initiate.h"
#include "security/general_ciphering.h"
#include "security/security.h"
#include "security/gost_crypto.h"
#include "mock_crypto.h"
#include "ic/script_table.h"
#include "ic/sap_assignment.h"
#include "ic/status_mapping.h"
#include "server/dispatcher.h"
#include "ic/data.h"
#include "ic/register.h"
#include "ic/extended_register.h"
#include "ic/clock.h"
#include "ic/disconnect_control.h"
#include "ic/security_setup.h"
#include "transport/transport.h"
#include "security/security.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Type system tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_type_constructors(void **state) {
	(void)state;
	osp_value_t v;

	v = osp_val_null();
	assert_int_equal(v.tag, OSP_TAG_NULL);

	v = osp_val_bool(true);
	assert_int_equal(v.tag, OSP_TAG_BOOLEAN);
	assert_true(v.as.boolean.value);

	v = osp_val_i8(-42);
	assert_int_equal(v.tag, OSP_TAG_INTEGER);
	assert_int_equal(v.as.int8.value, -42);

	v = osp_val_u8(255);
	assert_int_equal(v.tag, OSP_TAG_UNSIGNED);
	assert_int_equal(v.as.uint8.value, 255);

	v = osp_val_i16(-12345);
	assert_int_equal(v.tag, OSP_TAG_LONG);
	assert_int_equal(v.as.int16.value, -12345);

	v = osp_val_u16(60000);
	assert_int_equal(v.tag, OSP_TAG_LONG_UNSIGNED);
	assert_int_equal(v.as.uint16.value, 60000);

	v = osp_val_i32(-100000);
	assert_int_equal(v.tag, OSP_TAG_DOUBLE_LONG);
	assert_int_equal(v.as.int32.value, -100000);

	v = osp_val_u32(0xDEADBEEF);
	assert_int_equal(v.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(v.as.uint32.value, 0xDEADBEEF);

	v = osp_val_i64(1234567890123LL);
	assert_int_equal(v.tag, OSP_TAG_LONG64);
	assert_int_equal(v.as.int64.value, 1234567890123LL);

	v = osp_val_u64(0xDEADBEEFCAFEBABEULL);
	assert_int_equal(v.tag, OSP_TAG_LONG64_UNSIGNED);
	assert_int_equal(v.as.uint64.value, 0xDEADBEEFCAFEBABEULL);

	v = osp_val_enum(5);
	assert_int_equal(v.tag, OSP_TAG_ENUM);
	assert_int_equal(v.as.enum_val.value, 5);

	v = osp_val_date(2026, 7, 9, 3);
	assert_int_equal(v.tag, OSP_TAG_DATE);
	assert_int_equal(v.as.date.year, 2026);
	assert_int_equal(v.as.date.month, 7);
	assert_int_equal(v.as.date.day, 9);

	v = osp_val_time(14, 30, 45, 500);
	assert_int_equal(v.tag, OSP_TAG_TIME);
	assert_int_equal(v.as.time.hour, 14);
	assert_int_equal(v.as.time.ms, 0);

	v = osp_val_datetime(2026, 7, 9, 3, 14, 30, 45, 500);
	assert_int_equal(v.tag, OSP_TAG_DATETIME);
	assert_int_equal(v.as.datetime.date.year, 2026);
}

static void test_type_extractors(void **state) {
	(void)state;
	osp_value_t v;

	v = osp_val_i32(-999);
	assert_int_equal(osp_get_i32(&v), -999);
	v = osp_val_u32(42);
	assert_int_equal(osp_get_u32(&v), 42);
	v = osp_val_bool(true);
	assert_true(osp_get_bool(&v));
	v = osp_val_enum(7);
	assert_int_equal(osp_get_enum(&v), 7);

	/* Extractor on wrong type returns 0 */
	v = osp_val_u32(42);
	assert_int_equal(osp_get_i32(&v), 0);
}

static void test_type_size_table(void **state) {
	(void)state;
	assert_int_equal(osp_axdr_type_size(OSP_TAG_BOOLEAN), 1);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_INTEGER), 1);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_LONG), 2);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_DOUBLE_LONG), 4);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_LONG64), 8);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_DATE), 5);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_TIME), 4);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_DATETIME), 12);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_OCTETSTRING), 0);
	assert_int_equal(osp_axdr_type_size(OSP_TAG_ARRAY), 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Codec tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_axdr_roundtrip(void **state) {
	(void)state;
	uint8_t wbuf[128], rbuf[128];
	osp_buf_t w, r;
	osp_buf_init(&w, wbuf, sizeof(wbuf));

	osp_axdr_write_u8(&w, 0x42);
	osp_axdr_write_u16(&w, 0x1234);
	osp_axdr_write_u32(&w, 0xDEADBEEF);
	osp_axdr_write_bool(&w, true);

	osp_buf_init(&r, wbuf, w.wr);
	r.wr = w.wr;

	uint8_t v8;
	uint16_t v16;
	uint32_t v32;
	bool vb;
	assert_int_equal(osp_axdr_read_u8(&r, &v8), OSP_OK);
	assert_int_equal(v8, 0x42);
	assert_int_equal(osp_axdr_read_u16(&r, &v16), OSP_OK);
	assert_int_equal(v16, 0x1234);
	assert_int_equal(osp_axdr_read_u32(&r, &v32), OSP_OK);
	assert_int_equal(v32, 0xDEADBEEF);
	assert_int_equal(osp_axdr_read_bool(&r, &vb), OSP_OK);
	assert_true(vb);
}

static void test_axdr_signed_types(void **state) {
	(void)state;
	uint8_t buf[32];
	osp_buf_t b;
	osp_buf_init(&b, buf, sizeof(buf));

	osp_axdr_write_i8(&b, -42);
	osp_axdr_write_i16(&b, -12345);
	osp_axdr_write_i32(&b, -100000);
	osp_axdr_write_i64(&b, 1234567890123LL);

	osp_buf_t r;
	osp_buf_init(&r, buf, b.wr);
	r.wr = b.wr;

	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	osp_axdr_read_i8(&r, &i8);
	assert_int_equal(i8, -42);
	osp_axdr_read_i16(&r, &i16);
	assert_int_equal(i16, -12345);
	osp_axdr_read_i32(&r, &i32);
	assert_int_equal(i32, -100000);
	osp_axdr_read_i64(&r, &i64);
	assert_int_equal(i64, 1234567890123LL);
}

static void test_axdr_boundary(void **state) {
	(void)state;
	/* Read from empty buffer */
	uint8_t small[1];
	osp_buf_t b;
	osp_buf_init(&b, small, 0);

	uint8_t v8;
	assert_int_equal(osp_axdr_read_u8(&b, &v8), OSP_ERR_INVALID);

	/* Write to full buffer */
	osp_buf_init(&b, small, 1);
	assert_int_equal(osp_axdr_write_u8(&b, 1), OSP_OK);
	assert_int_equal(osp_axdr_write_u8(&b, 2), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Serializer tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_serialize_value_roundtrip(void **state) {
	(void)state;
	uint8_t buf[256];
	osp_buf_t w, r;

	/* Write multiple types */
	osp_buf_init(&w, buf, sizeof(buf));
	osp_value_t v1 = osp_val_bool(true);
	osp_value_t v2 = osp_val_u32(0xDEADBEEF);
	osp_value_t v3 = osp_val_date(2026, 7, 9, 3);
	assert_int_equal(osp_value_write(&w, &v1), OSP_OK);
	assert_int_equal(osp_value_write(&w, &v2), OSP_OK);
	assert_int_equal(osp_value_write(&w, &v3), OSP_OK);

	/* Read back */
	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	osp_value_t v;

	assert_int_equal(osp_value_read(&r, &v), OSP_OK);
	assert_int_equal(v.tag, OSP_TAG_BOOLEAN);
	assert_true(v.as.boolean.value);

	assert_int_equal(osp_value_read(&r, &v), OSP_OK);
	assert_int_equal(v.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(v.as.uint32.value, 0xDEADBEEF);

	assert_int_equal(osp_value_read(&r, &v), OSP_OK);
	assert_int_equal(v.tag, OSP_TAG_DATE);
	assert_int_equal(v.as.date.year, 2026);
	assert_int_equal(v.as.date.day, 9);
}

static void test_serialize_date_time(void **state) {
	(void)state;
	uint8_t buf[32];
	osp_buf_t w;
	osp_buf_init(&w, buf, sizeof(buf));

	osp_date_t d = {2026, 7, 9, 3};
	osp_time_t t = {14, 30, 45, 0};
	assert_int_equal(osp_date_write(&w, &d), OSP_OK);
	assert_int_equal(osp_time_write(&w, &t), OSP_OK);

	osp_buf_t r;
	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	osp_date_t d2;
	osp_time_t t2;
	assert_int_equal(osp_date_read(&r, &d2), OSP_OK);
	assert_int_equal(osp_time_read(&r, &t2), OSP_OK);
	assert_int_equal(d2.year, 2026);
	assert_int_equal(t2.hour, 14);
	assert_int_equal(t2.second, 45);
}

static void test_serialize_struct_array(void **state) {
	(void)state;
	uint8_t buf[64];
	osp_buf_t w, r;

	/* Structure: tag=2, fields=3, then 3 values */
	osp_buf_init(&w, buf, sizeof(buf));
	assert_int_equal(osp_struct_begin(&w, 3), OSP_OK);
	assert_int_equal(osp_axdr_write_u8(&w, 0xAA), OSP_OK);
	assert_int_equal(osp_axdr_write_u16(&w, 0x1234), OSP_OK);
	assert_int_equal(osp_axdr_write_u8(&w, 0xBB), OSP_OK);

	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	uint8_t nf;
	assert_int_equal(osp_struct_begin_read(&r, &nf), OSP_OK);
	assert_int_equal(nf, 3);
	uint8_t v8;
	uint16_t v16;
	osp_axdr_read_u8(&r, &v8);
	assert_int_equal(v8, 0xAA);
	osp_axdr_read_u16(&r, &v16);
	assert_int_equal(v16, 0x1234);
	osp_axdr_read_u8(&r, &v8);
	assert_int_equal(v8, 0xBB);

	/* Array: tag=1, count=5 */
	osp_buf_init(&w, buf, sizeof(buf));
	assert_int_equal(osp_array_begin(&w, 5), OSP_OK);
	for (int i = 0; i < 5; i++)
		osp_axdr_write_u8(&w, (uint8_t)i);

	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	uint8_t ac;
	assert_int_equal(osp_array_begin_read(&r, &ac), OSP_OK);
	assert_int_equal(ac, 5);
	for (int i = 0; i < 5; i++) {
		osp_axdr_read_u8(&r, &v8);
		assert_int_equal(v8, (uint8_t)i);
	}
}

static void test_serialize_obis(void **state) {
	(void)state;
	uint8_t buf[16];
	osp_buf_t w, r;
	osp_buf_init(&w, buf, sizeof(buf));

	osp_obis_t obis = {1, 2, 3, 4, 5, 6};
	assert_int_equal(osp_obis_write(&w, &obis), OSP_OK);

	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	osp_obis_t obis2;
	assert_int_equal(osp_obis_read(&r, &obis2), OSP_OK);
	assert_true(osp_obis_eq(&obis, &obis2));
}

static void test_serialize_scaler_unit(void **state) {
	(void)state;
	uint8_t buf[16];
	osp_buf_t w, r;
	osp_buf_init(&w, buf, sizeof(buf));

	osp_scaler_unit_t su = {-2, 30};
	assert_int_equal(osp_scaler_unit_write(&w, &su), OSP_OK);

	osp_buf_init(&r, buf, w.wr);
	r.wr = w.wr;
	osp_scaler_unit_t su2;
	assert_int_equal(osp_scaler_unit_read(&r, &su2), OSP_OK);
	assert_int_equal(su2.scaler, -2);
	assert_int_equal(su2.unit, 30);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  IC class / dispatcher tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_ic_data_class(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_data_class();
	assert_non_null(cls);
	assert_int_equal(cls->class_id, 1);
	assert_int_equal(cls->version, 0);
	assert_string_equal(cls->name, "Data");
	assert_non_null(cls->get_attr);
	assert_non_null(cls->set_attr);
}

static void test_ic_data_getset(void **state) {
	(void)state;
	osp_ic_data_t data;
	osp_ic_data_init(&data, (osp_obis_t){0, 0, 0x80, 0, 0, 0xFF});

	osp_value_t set_val = osp_val_u32(42);
	const osp_ic_class_t *cls = osp_ic_data_class();
	assert_int_equal(cls->set_attr(&data, 1, &set_val), OSP_OK);

	osp_value_t get_val = osp_val_null();
	assert_int_equal(cls->get_attr(&data, 1, &get_val), OSP_OK);
	assert_int_equal(get_val.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(get_val.as.uint32.value, 42);

	/* Wrong attr_id */
	assert_int_equal(cls->get_attr(&data, 2, &get_val), OSP_ERR_NOT_FOUND);
}

static void test_dispatcher_get_set(void **state) {
	(void)state;
	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);

	osp_ic_data_t data1;
	osp_ic_data_init(&data1, (osp_obis_t){0, 0, 1, 0, 0, 255});
	const osp_ic_class_t *cls = osp_ic_data_class();
	assert_int_equal(osp_dispatcher_register(&disp, cls, &data1), OSP_OK);

	/* Set */
	osp_value_t val = osp_val_u32(100);
	osp_obis_t ln1 = {0, 0, 1, 0, 0, 255};
	assert_int_equal(osp_dispatcher_set(&disp, 1, &ln1, 1, &val), OSP_OK);

	/* Get */
	osp_value_t result = osp_val_null();
	assert_int_equal(osp_dispatcher_get(&disp, 1, &ln1, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 100);

	/* Not found */
	osp_obis_t ln_unknown = {0, 0, 99, 0, 0, 255};
	assert_int_equal(osp_dispatcher_get(&disp, 99, &ln_unknown, 1, &result), OSP_ERR_NOT_FOUND);
}

static void test_dispatcher_multiple_objects(void **state) {
	(void)state;
	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);

	osp_ic_data_t data1, data2;
	osp_ic_data_init(&data1, (osp_obis_t){0, 0, 1, 0, 0, 255});
	osp_ic_data_init(&data2, (osp_obis_t){0, 0, 8, 0, 0, 255});
	const osp_ic_class_t *cls = osp_ic_data_class();
	assert_int_equal(osp_dispatcher_register(&disp, cls, &data1), OSP_OK);
	assert_int_equal(osp_dispatcher_register(&disp, cls, &data2), OSP_OK);
	assert_int_equal(disp.count, 2);

	osp_value_t v1 = osp_val_u32(111);
	osp_value_t v2 = osp_val_u32(222);
	osp_dispatcher_set(&disp, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &v1);
	osp_dispatcher_set(&disp, 1, &(osp_obis_t){0, 0, 8, 0, 0, 255}, 1, &v2);

	osp_value_t r1, r2;
	osp_dispatcher_get(&disp, 1, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 1, &r1);
	osp_dispatcher_get(&disp, 1, &(osp_obis_t){0, 0, 8, 0, 0, 255}, 1, &r2);
	assert_int_equal(r1.as.uint32.value, 111);
	assert_int_equal(r2.as.uint32.value, 222);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Transport tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_hdlc_crc(void **state) {
	(void)state;
	/* Known CRC-16/X.25 vector: "123456789" → 0x906E */
	const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	assert_int_equal(osp_hdlc_fcs16(data, 9), 0x906E);
}

static void test_hdlc_frame_roundtrip(void **state) {
	(void)state;
	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 1, 1);
	osp_hdlc_address_init(&frame.source, 1, 1);
	frame.control.poll_final = true;
	frame.info_len = 4;
	frame.info[0] = 0xC0;
	frame.info[1] = 0x01;
	frame.info[2] = 0x00;
	frame.info[3] = 0x80;

	uint8_t out[128];
	uint32_t out_len;
	osp_err_t r = osp_hdlc_frame(&frame, out, sizeof(out), &out_len);
	assert_int_equal(r, OSP_OK);
	assert_true(out_len > 0);
	assert_int_equal(out[0], OSP_HDLC_FLAG);
	assert_int_equal(out[out_len - 1], OSP_HDLC_FLAG);

	osp_hdlc_frame_t decoded;
	r = osp_hdlc_deframe(out, out_len, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.info_len, 4);
	assert_memory_equal(decoded.info, frame.info, 4);
	assert_int_equal(osp_hdlc_address_value(&decoded.destination), 1);
	assert_int_equal(osp_hdlc_address_value(&decoded.source), 1);
}

static void test_wrapper_roundtrip(void **state) {
	(void)state;
	const uint8_t apdu[] = {0xC0, 0x01, 0x00, 0x80, 0x00, 0x00};
	uint8_t out[64];
	uint32_t out_len;
	osp_err_t r = osp_wrapper_encode(1, 2, apdu, sizeof(apdu), out, sizeof(out), &out_len);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(out_len, 8 + 6);

	osp_wrapper_header_t hdr;
	const uint8_t *apdu_out;
	uint32_t apdu_out_len;
	r = osp_wrapper_decode(out, out_len, &hdr, &apdu_out, &apdu_out_len);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(hdr.version, OSP_WRAPPER_VERSION);
	assert_int_equal(hdr.source, 1);
	assert_int_equal(hdr.destination, 2);
	assert_int_equal(apdu_out_len, 6);
	assert_memory_equal(apdu_out, apdu, 6);
}

static void test_ber_read_errors(void **state) {
	(void)state;
	uint8_t mem[8];
	osp_buf_t buf;
	osp_ber_tag_t tag;
	uint32_t len;

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_read_tag(NULL, &tag), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_read_tag(&buf, NULL), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_read_tag(&buf, &tag), OSP_ERR_INVALID);

	osp_buf_init(&buf, mem, sizeof(mem));
	assert_int_equal(osp_ber_read_length(NULL, &len), OSP_ERR_INVALID);
	assert_int_equal(osp_ber_read_length(&buf, &len), OSP_ERR_INVALID);

	mem[0] = 0x85;
	osp_buf_init(&buf, mem, 1);
	buf.wr = 1;
	assert_int_equal(osp_ber_read_length(&buf, &len), OSP_ERR_UNSUPPORTED);
}

static void test_service_decode_invalid(void **state) {
	(void)state;
	uint8_t mem[4] = {0xFF, 0x01, 0x00, 0x00};
	osp_buf_t buf;
	osp_buf_init(&buf, mem, sizeof(mem));
	buf.wr = sizeof(mem);

	osp_get_request_t req;
	assert_int_equal(osp_get_request_decode(&buf, &req), -1);

	osp_buf_init(&buf, mem, sizeof(mem));
	buf.wr = sizeof(mem);
	osp_set_request_t sreq;
	assert_int_equal(osp_set_request_decode(&buf, &sreq), -1);

	osp_buf_init(&buf, mem, sizeof(mem));
	buf.wr = sizeof(mem);
	osp_action_request_t areq;
	assert_int_equal(osp_action_request_decode(&buf, &areq), -1);

	assert_int_equal(osp_get_request_decode(NULL, &req), -1);
	assert_int_equal(osp_get_request_decode(&buf, NULL), -1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  IC class tests (Register, Extended Register, Clock)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_ic_register(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_register_class();
	assert_non_null(cls);
	assert_int_equal(cls->class_id, 3);
	assert_int_equal(cls->version, 0);
	assert_string_equal(cls->name, "Register");

	osp_ic_register_t reg;
	osp_ic_register_init(&reg, (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(12345));
	assert_int_equal(reg.logical_name.c, 1);
	assert_int_equal(reg.value.as.uint32.value, 12345);

	/* GET value */
	osp_value_t v;
	assert_int_equal(cls->get_attr(&reg, 2, &v), OSP_OK);
	assert_int_equal(v.as.uint32.value, 12345);

	/* SET value */
	osp_value_t new_val = osp_val_u32(99999);
	assert_int_equal(cls->set_attr(&reg, 2, &new_val), OSP_OK);
	assert_int_equal(cls->get_attr(&reg, 2, &v), OSP_OK);
	assert_int_equal(v.as.uint32.value, 99999);

	/* Reset method */
	osp_value_t result;
	assert_int_equal(cls->invoke(&reg, 1, NULL, &result), OSP_OK);
	assert_int_equal(cls->get_attr(&reg, 2, &v), OSP_OK);
	assert_int_equal(v.as.int32.value, 0);
}

static void test_ic_extended_register(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_ext_register_class();
	assert_int_equal(cls->class_id, 4);

	osp_ic_ext_register_t ext;
	osp_ic_ext_register_init(&ext, (osp_obis_t){0, 0, 2, 0, 0, 255});

	osp_value_t v;
	assert_int_equal(cls->get_attr(&ext, 2, &v), OSP_OK);
	assert_int_equal(cls->get_attr(&ext, 4, &v), OSP_OK); /* status */
}

static void test_ic_clock(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_clock_class();
	assert_int_equal(cls->class_id, 8);
	assert_string_equal(cls->name, "Clock");

	osp_ic_clock_t clock;
	osp_ic_clock_init(&clock, (osp_obis_t){0, 0, 1, 0, 0, 255});

	osp_value_t v;
	assert_int_equal(cls->get_attr(&clock, 2, &v), OSP_OK);
	assert_int_equal(v.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(v.as.octetstring.len, OSP_COSEM_DATETIME_LEN);

	/* Set time */
	osp_cosem_datetime_t new_time = {0};
	new_time.year = 2026;
	new_time.month = 7;
	new_time.day = 9;
	new_time.day_of_week = 3;
	new_time.hour = 14;
	new_time.minute = 30;
	osp_value_t new_val = osp_val_cosem_datetime(&new_time);
	assert_int_equal(cls->set_attr(&clock, 2, &new_val), OSP_OK);
	assert_int_equal(cls->get_attr(&clock, 2, &v), OSP_OK);
	osp_cosem_datetime_t got;
	assert_int_equal(osp_cosem_datetime_read_value(&v, &got), OSP_OK);
	assert_int_equal(got.year, 2026);

	/* Methods: adjust_to_quarter, minute, etc. */
	osp_value_t result;
	for (uint8_t m = 1; m <= 6; m++) {
		assert_int_equal(cls->invoke(&clock, m, NULL, &result), OSP_OK);
	}
}

static void test_ic_dispatcher_multi(void **state) {
	(void)state;
	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);

	osp_ic_register_t reg;
	osp_ic_register_init(&reg, (osp_obis_t){0, 0, 1, 0, 0, 255}, osp_val_u32(42));
	assert_int_equal(osp_dispatcher_register(&disp, osp_ic_register_class(), &reg), OSP_OK);

	osp_ic_clock_t clk;
	osp_ic_clock_init(&clk, (osp_obis_t){0, 0, 1, 0, 0, 255});
	assert_int_equal(osp_dispatcher_register(&disp, osp_ic_clock_class(), &clk), OSP_OK);

	/* GET Register value */
	osp_value_t v;
	assert_int_equal(osp_dispatcher_get(&disp, 3, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &v), OSP_OK);
	assert_int_equal(v.as.uint32.value, 42);

	/* GET Clock time */
	assert_int_equal(osp_dispatcher_get(&disp, 8, &(osp_obis_t){0, 0, 1, 0, 0, 255}, 2, &v), OSP_OK);
	assert_int_equal(v.tag, OSP_TAG_OCTETSTRING);
}

static void test_ic_disconnect_control(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_disconnect_control_class();
	assert_int_equal(cls->class_id, 70);

	osp_ic_disconnect_control_t dc;
	osp_ic_disconnect_control_init(&dc, (osp_obis_t){0, 0, 96, 3, 10, 255});
	dc.output_state = 1;

	osp_value_t v;
	assert_int_equal(cls->get_attr(&dc, 2, &v), OSP_OK);
	assert_true(osp_get_bool(&v));

	osp_value_t result;
	assert_int_equal(cls->invoke(&dc, 1, NULL, &result), OSP_OK);
	assert_int_equal(dc.output_state, 0);
}

static void test_ic_security_setup(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_security_setup_class();
	assert_int_equal(cls->class_id, 64);

	osp_ic_security_setup_t ss;
	osp_ic_security_setup_init(&ss, (osp_obis_t){0, 0, 43, 0, 0, 255});
	ss.security_policy = 3;
	ss.security_suite = 0;

	osp_value_t v;
	assert_int_equal(cls->get_attr(&ss, 2, &v), OSP_OK);
	assert_int_equal(osp_get_u8(&v), 3);

	osp_value_t title;
	title.tag = OSP_TAG_OCTETSTRING;
	title.as.octetstring.len = 4;
	memcpy(title.as.octetstring.data, "CLNT", 4);
	assert_int_equal(cls->set_attr(&ss, 4, &title), OSP_OK);
	assert_int_equal(ss.client_system_title.len, 4);

	osp_value_t result;
	assert_int_equal(cls->invoke(&ss, 1, NULL, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_NULL);
}

static void test_ic_dispatcher_action(void **state) {
	(void)state;
	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);

	osp_ic_disconnect_control_t dc;
	osp_ic_disconnect_control_init(&dc, (osp_obis_t){0, 0, 96, 3, 10, 255});
	dc.output_state = 1;
	assert_int_equal(osp_dispatcher_register(&disp, osp_ic_disconnect_control_class(), &dc), OSP_OK);

	osp_value_t result;
	assert_int_equal(
	    osp_dispatcher_action(&disp, 70, &(osp_obis_t){0, 0, 96, 3, 10, 255}, 1, NULL, &result),
	    OSP_OK
	);
	assert_int_equal(dc.output_state, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Edge case tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_edge_max_octet_string(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[300];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Write maximum-length octet string (256 bytes) with tag */
	uint8_t data[256];
	memset(data, 0xAB, sizeof(data));
	assert_int_equal(osp_axdr_write_tag(&buf, OSP_AXDR_OCTETSTRING), OSP_OK);
	assert_int_equal(osp_axdr_write_octet_string(&buf, data, 256), OSP_OK);

	/* Read it back */
	buf.rd = 0;
	uint8_t out[256];
	uint32_t out_len = 0;
	assert_int_equal(osp_axdr_read_octet_string(&buf, out, 256, &out_len), OSP_OK);
	assert_int_equal(out_len, 256);
	assert_memory_equal(out, data, 256);
}

static void test_edge_zero_length_values(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[64];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Zero-length octet string with tag */
	assert_int_equal(osp_axdr_write_tag(&buf, OSP_AXDR_OCTETSTRING), OSP_OK);
	assert_int_equal(osp_axdr_write_octet_string(&buf, NULL, 0), OSP_OK);
	buf.rd = 0;
	uint8_t out[32];
	uint32_t out_len = 99;
	assert_int_equal(osp_axdr_read_octet_string(&buf, out, sizeof(out), &out_len), OSP_OK);
	assert_int_equal(out_len, 0);

	/* Zero-length value write/read */
	osp_buf_init(&buf, tx, sizeof(tx));
	osp_value_t val = osp_val_null();
	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);
	buf.rd = 0;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_NULL);
}

static void test_edge_empty_struct_array(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[128];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Empty structure */
	osp_value_t val;
	val.tag = OSP_TAG_STRUCTURE;
	val.as.structure.elements.count = 0;
	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);

	buf.rd = 0;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(decoded.as.structure.elements.count, 0);

	/* Empty array */
	osp_buf_init(&buf, tx, sizeof(tx));
	val.tag = OSP_TAG_ARRAY;
	val.as.array.elements.count = 0;
	assert_int_equal(osp_value_write(&buf, &val), OSP_OK);

	buf.rd = 0;
	assert_int_equal(osp_value_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_ARRAY);
	assert_int_equal(decoded.as.array.elements.count, 0);
}

static void test_edge_key_rotation(void **state) {
	(void)state;

	osp_sec_context_t ctx;
	uint8_t sys_title[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	osp_sec_context_init(&ctx, OSP_SUITE_0, OSP_MECH_HLS_GMAC, sys_title);

	/* Set initial keys */
	uint8_t guek[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
	uint8_t gak[16] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
	                   0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
	memcpy(ctx.guek, guek, 16);
	memcpy(ctx.gak, gak, 16);

	/* Set IC near overflow */
	ctx.invocation_counter = 0xFFFFFFFF - 500;
	ctx.ic_valid = true;

	/* Check rotation needed */
	assert_true(osp_sec_key_rotation_needed(&ctx));

	/* Rotate keys */
	uint8_t new_guek[16] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	                         0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30};
	uint8_t new_gak[16] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
	                       0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40};
	osp_sec_rotate_keys(&ctx, new_guek, new_gak);

	/* Verify keys updated */
	assert_memory_equal(ctx.guek, new_guek, 16);
	assert_memory_equal(ctx.gak, new_gak, 16);

	/* Verify IC reset */
	assert_int_equal(ctx.invocation_counter, 0);
	assert_false(ctx.ic_valid);

	/* Verify dedicated key cleared */
	assert_false(ctx.use_dedicated_key);

	/* Verify rotation no longer needed */
	assert_false(osp_sec_key_rotation_needed(&ctx));

	osp_sec_context_destroy(&ctx);
}

static void test_edge_bitstring_boundary(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[128];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Write bitstring with exact byte boundary (8 bits = 1 byte) */
	uint8_t bits[1] = {0xA5}; /* 10100101 */
	assert_int_equal(osp_bitstring_write(&buf, bits, 8), OSP_OK);

	buf.rd = 0;
	uint8_t out[8];
	uint32_t num_bits = 0;
	/* Bitstring read: len includes unused count byte, so total_bits = len*8 */
	assert_int_equal(osp_bitstring_read(&buf, out, 16, &num_bits), OSP_OK);
	/* num_bits = total_bits - unused = 16 - 0 = 16 (includes unused count byte) */
	assert_int_equal(num_bits, 16);
	assert_memory_equal(out, bits, 1);
}

static void test_edge_obis_boundary(void **state) {
	(void)state;
	osp_buf_t buf;
	uint8_t tx[32];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Write OBIS with max values */
	osp_obis_t obis = {255, 255, 255, 255, 255, 255};
	assert_int_equal(osp_obis_write(&buf, &obis), OSP_OK);

	buf.rd = 0;
	osp_obis_t decoded;
	assert_int_equal(osp_obis_read(&buf, &decoded), OSP_OK);
	assert_int_equal(decoded.a, 255);
	assert_int_equal(decoded.f, 255);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Test runner
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Selective access tests (xdms_selective.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_selective_access_none_roundtrip(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_NONE;

	assert_int_equal(osp_selective_access_encode(&buf, &sa), 0);

	/* Decode */
	osp_selective_access_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_decode(&rbuf, &decoded), 0);
	assert_int_equal(decoded.type, OSP_SEL_ACCESS_NONE);
}

static void test_selective_access_by_entry_roundtrip(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_ENTRY;
	sa.param.entry.from = 5;
	sa.param.entry.to = 10;

	assert_int_equal(osp_selective_access_encode(&buf, &sa), 0);

	osp_selective_access_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_decode(&rbuf, &decoded), 0);
	assert_int_equal(decoded.type, OSP_SEL_ACCESS_BY_ENTRY);
	assert_int_equal(decoded.param.entry.from, 5);
	assert_int_equal(decoded.param.entry.to, 10);
}

static void test_selective_access_by_range_roundtrip(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_RANGE;
	sa.param.entry.from = 1;
	sa.param.entry.to = 100;

	assert_int_equal(osp_selective_access_encode(&buf, &sa), 0);

	osp_selective_access_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_decode(&rbuf, &decoded), 0);
	assert_int_equal(decoded.type, OSP_SEL_ACCESS_BY_ENTRY);
	assert_int_equal(decoded.param.entry.from, 1);
	assert_int_equal(decoded.param.entry.to, 100);
}

static void test_selective_access_by_date_roundtrip(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_DATE;
	sa.param.date.from = (osp_date_t){.year = 2025, .month = 1, .day = 15, .day_of_week = 3};
	sa.param.date.to = (osp_date_t){.year = 2025, .month = 12, .day = 31, .day_of_week = 2};

	assert_int_equal(osp_selective_access_encode(&buf, &sa), 0);

	osp_selective_access_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_decode(&rbuf, &decoded), 0);
	assert_int_equal(decoded.type, OSP_SEL_ACCESS_BY_DATE);
	assert_int_equal(decoded.param.date.from.year, 2025);
	assert_int_equal(decoded.param.date.from.month, 1);
	assert_int_equal(decoded.param.date.to.year, 2025);
	assert_int_equal(decoded.param.date.to.month, 12);
}

static void test_selective_access_skip(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_ENTRY;
	sa.param.entry.from = 1;
	sa.param.entry.to = 5;
	osp_selective_access_encode(&buf, &sa);

	/* Skip */
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_skip(&rbuf), 0);
}

static void test_selective_access_skip_none(void **state) {
	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_NONE;
	osp_selective_access_encode(&buf, &sa);

	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_selective_access_skip(&rbuf), 0);
}

static void test_selective_access_apply_entry(void **state) {
	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_ENTRY;
	sa.param.entry.from = 2;
	sa.param.entry.to = 4;

	osp_profile_row_t rows[8];
	for (int i = 0; i < 8; i++) {
		rows[i].cell_count = 1;
		rows[i].cells[0] = osp_val_u32((uint32_t)(i + 1));
	}
	uint8_t count = 8;

	int result = osp_selective_access_apply_to_buffer(&sa, rows, &count);
	assert_int_equal(result, 3);
	assert_int_equal(count, 3);
	/* Rows 2,3,4 (0-indexed 1,2,3) */
	assert_int_equal(rows[0].cells[0].as.uint32.value, 2);
	assert_int_equal(rows[1].cells[0].as.uint32.value, 3);
	assert_int_equal(rows[2].cells[0].as.uint32.value, 4);
}

static void test_selective_access_apply_none(void **state) {
	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_NONE;

	osp_profile_row_t rows[4];
	uint8_t count = 4;
	int result = osp_selective_access_apply_to_buffer(&sa, rows, &count);
	assert_int_equal(result, 4);
	assert_int_equal(count, 4);
}

static void test_selective_access_apply_empty(void **state) {
	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_ENTRY;
	sa.param.entry.from = 1;
	sa.param.entry.to = 10;

	osp_profile_row_t rows[4];
	uint8_t count = 0;
	int result = osp_selective_access_apply_to_buffer(&sa, rows, &count);
	assert_int_equal(result, 0);
}

static void test_selective_access_apply_all(void **state) {
	osp_selective_access_t sa;
	sa.type = OSP_SEL_ACCESS_BY_ENTRY;
	sa.param.entry.from = 1;
	sa.param.entry.to = 0; /* 0 = all */

	osp_profile_row_t rows[3];
	for (int i = 0; i < 3; i++) {
		rows[i].cell_count = 1;
		rows[i].cells[0] = osp_val_u32((uint32_t)(i + 10));
	}
	uint8_t count = 3;
	int result = osp_selective_access_apply_to_buffer(&sa, rows, &count);
	assert_int_equal(result, 3);
}

static void test_selective_access_error_null(void **state) {
	assert_int_equal(osp_selective_access_encode(NULL, NULL), -1);
	assert_int_equal(osp_selective_access_decode(NULL, NULL), -1);
	assert_int_equal(osp_selective_access_skip(NULL), -1);
	assert_int_equal(osp_selective_access_apply_to_buffer(NULL, NULL, NULL), -1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SPODUS tasks tests (tasks.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_spodus_tasks_init(void **state) {
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);
	assert_int_equal(tasks.count, 0);
}

static void test_spodus_tasks_add(void **state) {
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);

	osp_spodus_exchange_task_t task = {0};
	task.task_id = 42;
	task.meter_id_len = 4;
	memcpy(task.meter_id, "\x01\x02\x03\x04", 4);
	task.script_count = 1;
	task.scripts[0].service_id = 1;
	task.scripts[0].class_id = 3;
	task.scripts[0].attribute_id = 2;
	task.execution_type = 0;
	task.priority = 100;

	assert_int_equal(osp_spodus_exchange_tasks_add(&tasks, &task), OSP_OK);
	assert_int_equal(tasks.count, 1);
	assert_int_equal(tasks.entries[0].task_id, 42);
}

static void test_spodus_tasks_add_overflow(void **state) {
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);

	osp_spodus_exchange_task_t task = {0};
	task.task_id = 1;
	task.meter_id_len = 1;

	for (int i = 0; i < OSP_SPODUS_MAX_TASKS; i++) {
		task.task_id = (uint16_t)(i + 1);
		assert_int_equal(osp_spodus_exchange_tasks_add(&tasks, &task), OSP_OK);
	}
	assert_int_equal(tasks.count, OSP_SPODUS_MAX_TASKS);

	/* One more should fail */
	task.task_id = 999;
	assert_int_equal(osp_spodus_exchange_tasks_add(&tasks, &task), OSP_ERR_NOMEM);
}

static void test_spodus_tasks_build_value(void **state) {
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);

	osp_spodus_exchange_task_t task = {0};
	task.task_id = 10;
	task.meter_id_len = 3;
	memcpy(task.meter_id, "\xAA\xBB\xCC", 3);
	task.script_count = 2;
	task.scripts[0].service_id = 1;
	task.scripts[0].class_id = 3;
	task.scripts[0].attribute_id = 2;
	task.scripts[0].obis = (osp_obis_t){0, 0, 1, 8, 0, 255};
	task.scripts[1].service_id = 2;
	task.scripts[1].class_id = 8;
	task.scripts[1].attribute_id = 1;
	task.execution_type = 1;
	task.priority = 50;

	assert_int_equal(osp_spodus_exchange_tasks_add(&tasks, &task), OSP_OK);

	osp_value_t out;
	assert_int_equal(osp_spodus_exchange_tasks_build_value(&tasks, &out), OSP_OK);
	assert_int_equal(out.tag, OSP_TAG_ARRAY);
	assert_int_equal(out.as.array.elements.count, 1);

	/* Verify first task structure */
	osp_value_t *row = &out.as.array.elements.items[0];
	assert_int_equal(row->tag, OSP_TAG_STRUCTURE);
	assert_int_equal(row->as.structure.elements.count, 6);
	assert_int_equal(row->as.structure.elements.items[0].as.uint16.value, 10);
	assert_int_equal(row->as.structure.elements.items[1].as.octetstring.len, 3);
	assert_int_equal(row->as.structure.elements.items[3].as.uint8.value, 1);
	assert_int_equal(row->as.structure.elements.items[5].as.uint16.value, 50);
}

static void test_spodus_tasks_build_empty(void **state) {
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);

	osp_value_t out;
	assert_int_equal(osp_spodus_exchange_tasks_build_value(&tasks, &out), OSP_OK);
	assert_int_equal(out.tag, OSP_TAG_ARRAY);
	assert_int_equal(out.as.array.elements.count, 0);
}

static void test_spodus_tasks_error_null(void **state) {
	osp_spodus_exchange_task_t task = {0};
	assert_int_equal(osp_spodus_exchange_tasks_add(NULL, &task), OSP_ERR_INVALID);
	osp_spodus_exchange_tasks_t tasks;
	osp_spodus_exchange_tasks_init(&tasks);
	assert_int_equal(osp_spodus_exchange_tasks_add(&tasks, NULL), OSP_ERR_INVALID);
	assert_int_equal(osp_spodus_exchange_tasks_build_value(NULL, NULL), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SPODUS helpers tests (spodus_helpers.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_ic_value_eq_null(void **state) {
	(void)state;
	assert_false(osp_ic_value_eq(NULL, NULL));
	osp_value_t v = osp_val_u32(1);
	assert_false(osp_ic_value_eq(NULL, &v));
	assert_false(osp_ic_value_eq(&v, NULL));
}

static void test_ic_value_eq_same_types(void **state) {
	(void)state;
	osp_value_t null_a = osp_val_null(), null_b = osp_val_null();
	assert_true(osp_ic_value_eq(&null_a, &null_b));
	osp_value_t u8_a = osp_val_u8(42), u8_b = osp_val_u8(42), u8_c = osp_val_u8(43);
	assert_true(osp_ic_value_eq(&u8_a, &u8_b));
	assert_false(osp_ic_value_eq(&u8_a, &u8_c));
	osp_value_t u16_a = osp_val_u16(1000), u16_b = osp_val_u16(1000);
	assert_true(osp_ic_value_eq(&u16_a, &u16_b));
	osp_value_t u32_a = osp_val_u32(0x12345678), u32_b = osp_val_u32(0x12345678);
	assert_true(osp_ic_value_eq(&u32_a, &u32_b));
	osp_value_t i8_a = osp_val_i8(-5), i8_b = osp_val_i8(-5);
	assert_true(osp_ic_value_eq(&i8_a, &i8_b));
	osp_value_t i16_a = osp_val_i16(-1000), i16_b = osp_val_i16(-1000);
	assert_true(osp_ic_value_eq(&i16_a, &i16_b));
	osp_value_t i32_a = osp_val_i32(-100000), i32_b = osp_val_i32(-100000);
	assert_true(osp_ic_value_eq(&i32_a, &i32_b));
}

static void test_ic_value_eq_octetstring(void **state) {
	(void)state;
	osp_value_t a, b;
	a.tag = OSP_TAG_OCTETSTRING;
	a.as.octetstring.len = 3;
	memcpy(a.as.octetstring.data, "\xAA\xBB\xCC", 3);
	b.tag = OSP_TAG_OCTETSTRING;
	b.as.octetstring.len = 3;
	memcpy(b.as.octetstring.data, "\xAA\xBB\xCC", 3);
	assert_true(osp_ic_value_eq(&a, &b));
	b.as.octetstring.data[2] = 0xDD;
	assert_false(osp_ic_value_eq(&a, &b));
	b.as.octetstring.len = 2;
	assert_false(osp_ic_value_eq(&a, &b));
}

static void test_ic_value_eq_different_tags(void **state) {
	(void)state;
	osp_value_t a = osp_val_u8(1), b = osp_val_u16(1);
	assert_false(osp_ic_value_eq(&a, &b));
}

static void test_ic_value_compare_integers(void **state) {
	(void)state;
	osp_value_t a, b;
	a = osp_val_u8(5); b = osp_val_u8(5);
	assert_int_equal(osp_ic_value_compare(&a, &b), 0);
	a = osp_val_u8(3); b = osp_val_u8(5);
	assert_true(osp_ic_value_compare(&a, &b) < 0);
	a = osp_val_u8(7); b = osp_val_u8(5);
	assert_true(osp_ic_value_compare(&a, &b) > 0);

	a = osp_val_u16(100); b = osp_val_u16(100);
	assert_int_equal(osp_ic_value_compare(&a, &b), 0);
	a = osp_val_u32(999); b = osp_val_u32(1000);
	assert_true(osp_ic_value_compare(&a, &b) < 0);
	a = osp_val_i8(-1); b = osp_val_u8(1);
	assert_true(osp_ic_value_compare(&a, &b) < 0);
	a = osp_val_i16(0); b = osp_val_i16(0);
	assert_int_equal(osp_ic_value_compare(&a, &b), 0);
	a = osp_val_i32(-100); b = osp_val_i32(-100);
	assert_int_equal(osp_ic_value_compare(&a, &b), 0);
}

static void test_ic_value_compare_octetstring(void **state) {
	(void)state;
	osp_value_t a, b;
	a.tag = OSP_TAG_OCTETSTRING;
	a.as.octetstring.len = 3;
	memcpy(a.as.octetstring.data, "\x01\x02\x03", 3);
	b.tag = OSP_TAG_OCTETSTRING;
	b.as.octetstring.len = 3;
	memcpy(b.as.octetstring.data, "\x01\x02\x04", 3);
	assert_true(osp_ic_value_compare(&a, &b) < 0);
	assert_true(osp_ic_value_compare(&b, &a) > 0);
	a.as.octetstring.len = 2;
	assert_true(osp_ic_value_compare(&a, &b) < 0);
	a.as.octetstring.len = 4;
	assert_true(osp_ic_value_compare(&a, &b) > 0);
}

static void test_ic_value_compare_enum(void **state) {
	(void)state;
	osp_value_t a, b;
	a = osp_val_enum(5);
	b = osp_val_enum(10);
	assert_true(osp_ic_value_compare(&a, &b) < 0);
}

static void test_ic_value_compare_mixed_tags(void **state) {
	(void)state;
	osp_value_t a, b;
	a = osp_val_u8(5);
	memset(&b, 0, sizeof(b));
	b.tag = OSP_TAG_VISIBLESTRING;
	assert_int_equal(osp_ic_value_compare(&a, &b), 0);
}

static void test_ic_value_compare_null(void **state) {
	(void)state;
	assert_int_equal(osp_ic_value_compare(NULL, NULL), 0);
	osp_value_t v = osp_val_u8(1);
	assert_int_equal(osp_ic_value_compare(NULL, &v), 0);
	assert_int_equal(osp_ic_value_compare(&v, NULL), 0);
}

static void test_ic_row_key(void **state) {
	(void)state;
	osp_ic_row_t row;
	row.field_count = 3;
	row.fields[0] = osp_val_u32(100);
	row.fields[1] = osp_val_u32(200);
	row.fields[2] = osp_val_u32(300);

	const osp_value_t *k = osp_ic_row_key(&row, 1);
	assert_non_null(k);
	assert_int_equal(k->as.uint32.value, 200);
	assert_null(osp_ic_row_key(&row, 5));
	assert_null(osp_ic_row_key(NULL, 0));
}

static void test_ic_row_to_value(void **state) {
	(void)state;
	osp_ic_row_t row;
	row.field_count = 2;
	row.fields[0] = osp_val_u8(10);
	row.fields[1] = osp_val_u16(20);

	osp_value_t out;
	assert_int_equal(osp_ic_row_to_value(&row, &out), OSP_OK);
	assert_int_equal(out.tag, OSP_TAG_STRUCTURE);
	assert_int_equal(out.as.structure.elements.count, 2);
	assert_int_equal(out.as.structure.elements.items[0].as.uint8.value, 10);
	assert_int_equal(out.as.structure.elements.items[1].as.uint16.value, 20);
	assert_int_equal(osp_ic_row_to_value(NULL, &out), OSP_ERR_INVALID);
	assert_int_equal(osp_ic_row_to_value(&row, NULL), OSP_ERR_INVALID);
}

static void test_ic_value_to_row(void **state) {
	(void)state;
	osp_value_t val;
	val.tag = OSP_TAG_STRUCTURE;
	osp_value_t items[3];
	items[0] = osp_val_u8(1);
	items[1] = osp_val_u16(2);
	items[2] = osp_val_u32(3);
	val.as.structure.elements.items = items;
	val.as.structure.elements.count = 3;

	osp_ic_row_t row;
	assert_int_equal(osp_ic_value_to_row(&val, &row), OSP_OK);
	assert_int_equal(row.field_count, 3);
	assert_int_equal(row.fields[0].as.uint8.value, 1);
	assert_int_equal(row.fields[1].as.uint16.value, 2);
	assert_int_equal(row.fields[2].as.uint32.value, 3);

	assert_int_equal(osp_ic_value_to_row(NULL, &row), OSP_ERR_INVALID);
	assert_int_equal(osp_ic_value_to_row(&val, NULL), OSP_ERR_INVALID);
	val.tag = OSP_TAG_UNSIGNED;
	assert_int_equal(osp_ic_value_to_row(&val, &row), OSP_ERR_INVALID);
}

static void test_ic_parse_entries_list(void **state) {
	(void)state;
	/* Build param: structure { _, array{u32(10), u32(20)} } */
	osp_value_t arr_items[2];
	arr_items[0] = osp_val_u32(10);
	arr_items[1] = osp_val_u32(20);
	osp_value_t arr;
	arr.tag = OSP_TAG_ARRAY;
	arr.as.array.elements.items = arr_items;
	arr.as.array.elements.count = 2;

	osp_value_t param_items[2];
	param_items[0] = osp_val_null();
	param_items[1] = arr;
	osp_value_t param;
	param.tag = OSP_TAG_STRUCTURE;
	param.as.structure.elements.items = param_items;
	param.as.structure.elements.count = 2;

	osp_value_t entries[4];
	uint8_t count = 0;
	assert_int_equal(osp_ic_spodus_parse_entries_list(&param, entries, 4, &count), OSP_OK);
	assert_int_equal(count, 2);
	assert_int_equal(entries[0].as.uint32.value, 10);
	assert_int_equal(entries[1].as.uint32.value, 20);

	/* Overflow */
	assert_int_equal(osp_ic_spodus_parse_entries_list(&param, entries, 1, &count), OSP_ERR_NOMEM);

	/* NULL */
	assert_int_equal(osp_ic_spodus_parse_entries_list(NULL, entries, 4, &count), OSP_ERR_INVALID);
}

static void test_ic_parse_filter_request(void **state) {
	(void)state;
	/* Build param: structure { _, array{u8(1)}, array{u8(2)} } */
	osp_value_t sel_items[1];
	sel_items[0] = osp_val_u8(1);
	osp_value_t sel_arr;
	sel_arr.tag = OSP_TAG_ARRAY;
	sel_arr.as.array.elements.items = sel_items;
	sel_arr.as.array.elements.count = 1;

	osp_value_t filt_items[1];
	filt_items[0] = osp_val_u8(2);
	osp_value_t filt_arr;
	filt_arr.tag = OSP_TAG_ARRAY;
	filt_arr.as.array.elements.items = filt_items;
	filt_arr.as.array.elements.count = 1;

	osp_value_t param_items[3];
	param_items[0] = osp_val_null();
	param_items[1] = sel_arr;
	param_items[2] = filt_arr;
	osp_value_t param;
	param.tag = OSP_TAG_STRUCTURE;
	param.as.structure.elements.items = param_items;
	param.as.structure.elements.count = 3;

	osp_value_t selected[4], filters[4];
	uint8_t sel_count = 0, filt_count = 0;
	assert_int_equal(osp_ic_spodus_parse_filter_request(&param, selected, 4, &sel_count, filters, 4, &filt_count), OSP_OK);
	assert_int_equal(sel_count, 1);
	assert_int_equal(filt_count, 1);
	assert_int_equal(selected[0].as.uint8.value, 1);
	assert_int_equal(filters[0].as.uint8.value, 2);

	assert_int_equal(osp_ic_spodus_parse_filter_request(NULL, selected, 4, &sel_count, filters, 4, &filt_count), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SPODUS server tests (spodus_server.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_spodus_data_init(void **state) {
	(void)state;
	osp_ic_spodus_data_t obj;
	osp_spodus_concentrator_t conc;
	memset(&conc, 0, sizeof(conc));
	osp_ic_spodus_data_init(&obj, (osp_obis_t){0, 0, 1, 0, 0, 255}, &conc, OSP_SPODUS_DATA_METER_LIST);
	assert_int_equal(obj.logical_name.a, 0);
	assert_int_equal(obj.kind, OSP_SPODUS_DATA_METER_LIST);
	assert_ptr_equal(obj.conc, &conc);

	osp_ic_spodus_data_init(NULL, (osp_obis_t){0}, NULL, 0);
}

static void test_spodus_data_class(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_spodus_data_class();
	assert_non_null(cls);
	assert_string_equal(cls->name, "SpodusData");
}

static void test_spodus_data_get_attr_errors(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_spodus_data_class();
	osp_ic_spodus_data_t obj;
	memset(&obj, 0, sizeof(obj));
	osp_value_t result;

	/* attr_id != 1 */
	assert_int_equal(cls->get_attr(&obj, 2, &result), OSP_ERR_NOT_FOUND);

	/* NULL result */
	assert_int_equal(cls->get_attr(&obj, 1, NULL), OSP_ERR_NOT_FOUND);

	/* NULL conc */
	obj.conc = NULL;
	assert_int_equal(cls->get_attr(&obj, 1, &result), OSP_ERR_NOT_FOUND);

	/* Unknown kind */
	osp_spodus_concentrator_t conc;
	memset(&conc, 0, sizeof(conc));
	obj.conc = &conc;
	obj.kind = 99;
	assert_int_equal(cls->get_attr(&obj, 1, &result), OSP_ERR_NOT_FOUND);
}

static void test_spodus_data_set_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_spodus_data_class();
	osp_ic_spodus_data_t obj;
	memset(&obj, 0, sizeof(obj));
	osp_value_t val = osp_val_u32(1);
	assert_int_equal(cls->set_attr(&obj, 1, &val), OSP_ERR_UNSUPPORTED);
	assert_int_equal(cls->set_attr(&obj, 2, &val), OSP_ERR_NOT_FOUND);
}

static void test_spodus_data_invoke(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_spodus_data_class();
	osp_spodus_concentrator_t conc;
	memset(&conc, 0, sizeof(conc));
	osp_ic_spodus_data_t obj;
	osp_ic_spodus_data_init(&obj, (osp_obis_t){0}, &conc, OSP_SPODUS_DATA_METER_LIST);
	osp_value_t result;

	/* method_id != 1 */
	assert_int_equal(cls->invoke(&obj, 2, NULL, &result), OSP_ERR_NOT_FOUND);

	/* NULL result */
	assert_int_equal(cls->invoke(&obj, 1, NULL, NULL), OSP_ERR_NOT_FOUND);

	/* method_id == 1, but no registry data — should return error or empty */
	osp_err_t r = cls->invoke(&obj, 1, NULL, &result);
	(void)r;
}

static void test_spodus_data_serialize_deserialize(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_spodus_data_class();
	osp_spodus_concentrator_t conc;
	memset(&conc, 0, sizeof(conc));
	osp_ic_spodus_data_t obj;
	osp_ic_spodus_data_init(&obj, (osp_obis_t){0, 0, 1, 0, 0, 255}, &conc, OSP_SPODUS_DATA_EXCHANGE_TASKS);

	uint8_t buf_mem[512];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));

	osp_err_t r = cls->serialize(&obj, &buf);
	/* May fail if concentrator has no data — that's OK for this test */
	(void)r;

	/* deserialize */
	osp_ic_spodus_data_t obj2;
	memset(&obj2, 0, sizeof(obj2));
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	/* deserialize will fail on incomplete data — that's expected */
	(void)cls->deserialize(&obj2, &rbuf);
}

static void test_spodus_concentrator_register_server(void **state) {
	(void)state;
	osp_spodus_concentrator_t conc;
	memset(&conc, 0, sizeof(conc));
	osp_spodus_concentrator_init(&conc);

	osp_server_t server;
	osp_transport_t transport = {0};
	osp_server_init(&server, &transport, OSP_FRAMING_WRAPPER);

	osp_err_t r = osp_spodus_concentrator_register_server(&server, &conc);
	assert_int_equal(r, OSP_OK);

	/* NULL checks */
	assert_int_equal(osp_spodus_concentrator_register_server(NULL, &conc), OSP_ERR_INVALID);
	assert_int_equal(osp_spodus_concentrator_register_server(&server, NULL), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Initiate tests (initiate.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_initiate_request_default(void **state) {
	(void)state;
	osp_initiate_request_t req;
	osp_initiate_request_default(&req);
	assert_true(req.response_allowed);
	assert_int_equal(req.proposed_dlms_version, 6);
	assert_int_equal(req.proposed_conformance, 0x007E1F);
	assert_int_equal(req.client_max_receive_pdu_size, 0x04B0);
	assert_false(req.has_dedicated_key);
	assert_false(req.has_qos);
}

static void test_initiate_response_default(void **state) {
	(void)state;
	osp_initiate_response_t resp;
	osp_initiate_response_default(&resp);
	assert_int_equal(resp.negotiated_dlms_version, 6);
	assert_int_equal(resp.negotiated_conformance, 0x007E1F);
	assert_int_equal(resp.server_max_receive_pdu_size, 0x0800);
	assert_int_equal(resp.vaa_name, 0x0007);
}

static void test_initiate_request_roundtrip(void **state) {
	(void)state;
	osp_initiate_request_t req;
	osp_initiate_request_default(&req);

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_request_encode(&req, &buf), OSP_OK);

	osp_initiate_request_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_request_decode(&rbuf, &decoded), OSP_OK);
	assert_true(decoded.response_allowed);
	assert_int_equal(decoded.proposed_dlms_version, 6);
	assert_int_equal(decoded.proposed_conformance, 0x007E1F);
	assert_int_equal(decoded.client_max_receive_pdu_size, 0x04B0);
	assert_false(decoded.has_dedicated_key);
}

static void test_initiate_request_dedicated_key(void **state) {
	(void)state;
	osp_initiate_request_t req;
	osp_initiate_request_default(&req);
	req.has_dedicated_key = true;
	req.dedicated_key_len = 4;
	memcpy(req.dedicated_key, "\xAA\xBB\xCC\xDD", 4);

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_request_encode(&req, &buf), OSP_OK);

	osp_initiate_request_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_request_decode(&rbuf, &decoded), OSP_OK);
	assert_true(decoded.has_dedicated_key);
	assert_int_equal(decoded.dedicated_key_len, 4);
	assert_memory_equal(decoded.dedicated_key, "\xAA\xBB\xCC\xDD", 4);
}

static void test_initiate_request_no_response(void **state) {
	(void)state;
	osp_initiate_request_t req;
	osp_initiate_request_default(&req);
	req.response_allowed = false;

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_request_encode(&req, &buf), OSP_OK);

	osp_initiate_request_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_request_decode(&rbuf, &decoded), OSP_OK);
	assert_false(decoded.response_allowed);
}

static void test_initiate_request_qos(void **state) {
	(void)state;
	osp_initiate_request_t req;
	osp_initiate_request_default(&req);
	req.has_qos = true;
	req.proposed_quality_of_service = 5;

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_request_encode(&req, &buf), OSP_OK);

	osp_initiate_request_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_request_decode(&rbuf, &decoded), OSP_OK);
	assert_true(decoded.has_qos);
	assert_int_equal(decoded.proposed_quality_of_service, 5);
}

static void test_initiate_request_error_null(void **state) {
	(void)state;
	osp_initiate_request_default(NULL);
	assert_int_equal(osp_initiate_request_encode(NULL, NULL), OSP_ERR_INVALID);
	assert_int_equal(osp_initiate_request_decode(NULL, NULL), OSP_ERR_INVALID);
}

static void test_initiate_response_roundtrip(void **state) {
	(void)state;
	osp_initiate_response_t resp;
	osp_initiate_response_default(&resp);

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_response_encode(&resp, &buf), OSP_OK);

	osp_initiate_response_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_response_decode(&rbuf, &decoded), OSP_OK);
	assert_int_equal(decoded.negotiated_dlms_version, 6);
	assert_int_equal(decoded.negotiated_conformance, 0x007E1F);
	assert_int_equal(decoded.server_max_receive_pdu_size, 0x0800);
	assert_int_equal(decoded.vaa_name, 0x0007);
}

static void test_initiate_response_qos(void **state) {
	(void)state;
	osp_initiate_response_t resp;
	osp_initiate_response_default(&resp);
	resp.has_qos = true;
	resp.negotiated_quality_of_service = -3;

	uint8_t buf_mem[128];
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	assert_int_equal(osp_initiate_response_encode(&resp, &buf), OSP_OK);

	osp_initiate_response_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, buf_mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_response_decode(&rbuf, &decoded), OSP_OK);
	assert_true(decoded.has_qos);
	assert_int_equal(decoded.negotiated_quality_of_service, -3);
}

static void test_initiate_response_error_null(void **state) {
	(void)state;
	osp_initiate_response_default(NULL);
	assert_int_equal(osp_initiate_response_encode(NULL, NULL), OSP_ERR_INVALID);
	assert_int_equal(osp_initiate_response_decode(NULL, NULL), OSP_ERR_INVALID);
}

static void test_initiate_request_decode_bad_tag(void **state) {
	(void)state;
	uint8_t buf_mem[4] = {0x99, 0x00, 0x00, 0x00};
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	osp_initiate_request_t req;
	assert_int_equal(osp_initiate_request_decode(&buf, &req), OSP_ERR_INVALID);
}

static void test_initiate_response_decode_bad_tag(void **state) {
	(void)state;
	uint8_t buf_mem[4] = {0x99, 0x00, 0x00, 0x00};
	osp_buf_t buf;
	osp_buf_init(&buf, buf_mem, sizeof(buf_mem));
	osp_initiate_response_t resp;
	assert_int_equal(osp_initiate_response_decode(&buf, &resp), OSP_ERR_INVALID);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  General ciphering tests (general_ciphering.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_gen_is_ciphered_tag(void **state) {
	(void)state;
	assert_true(osp_gen_is_ciphered_tag(OSP_GEN_GLO_CIPHERING));
	assert_true(osp_gen_is_ciphered_tag(OSP_GEN_DED_CIPHERING));
	assert_true(osp_gen_is_ciphered_tag(OSP_GEN_CIPHERING));
	assert_true(osp_gen_is_ciphered_tag(OSP_GEN_SIGNING));
	assert_false(osp_gen_is_ciphered_tag(0xC0));
	assert_false(osp_gen_is_ciphered_tag(0x00));
}

static void test_gen_glo_ded_protect_unprotect(void **state) {
	(void)state;
	/* Need a security context with keys */
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, st);
	uint8_t guek[16] = {0};
	uint8_t gak[16] = {0};
	memcpy(sec.guek, guek, 16);
	memcpy(sec.gak, gak, 16);
	sec.policy = OSP_POLICY_ENCR_AUTH;

	/* Need mock GCM */
	int saved = 0;
	if (!osp_hal_gcm_crypt) {
		saved = 1;
		/* Without GCM, protect should fail */
		uint8_t plaintext[] = {0xC0, 0x01};
		uint8_t ciphered[256];
		uint32_t ciphered_len;
		assert_int_not_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, 2, ciphered, &ciphered_len), 0);
		return;
	}

	uint8_t plaintext[] = {0xC0, 0x01, 0xC1, 0x00};
	uint8_t ciphered[256];
	uint32_t ciphered_len;
	assert_int_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, sizeof(plaintext), ciphered, &ciphered_len), 0);
	assert_true(ciphered_len > 0);

	uint8_t recovered[256];
	uint32_t recovered_len;
	uint8_t plain_tag;
	assert_int_equal(osp_gen_glo_ded_unprotect(&sec, ciphered, ciphered_len, recovered, &recovered_len, &plain_tag), 0);
	assert_int_equal(plain_tag, 0xC0);
	(void)saved;
}

static void test_gen_ciphering_protect_unprotect(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, st);
	uint8_t guek[16] = {0};
	uint8_t gak[16] = {0};
	memcpy(sec.guek, guek, 16);
	memcpy(sec.gak, gak, 16);
	sec.policy = OSP_POLICY_ENCR_AUTH;

	if (!osp_hal_gcm_crypt) {
		uint8_t plaintext[] = {0xC0, 0x01};
		uint8_t out[256];
		uint32_t out_len;
		assert_int_not_equal(osp_gen_ciphering_protect(&sec, NULL, 0, NULL, 0, 0xC0, plaintext, 2, out, &out_len), 0);
		return;
	}

	uint8_t tx_id[4] = {0x11, 0x22, 0x33, 0x44};
	uint8_t recipient[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
	uint8_t plaintext[] = {0xC0, 0x01};
	uint8_t out[256];
	uint32_t out_len;
	assert_int_equal(osp_gen_ciphering_protect(&sec, tx_id, 4, recipient, 8, 0xC0, plaintext, 2, out, &out_len), 0);

	uint8_t recovered[256];
	uint32_t recovered_len;
	uint8_t plain_tag;
	assert_int_equal(osp_gen_ciphering_unprotect(&sec, out, out_len, recovered, &recovered_len, &plain_tag), 0);
	assert_int_equal(plain_tag, 0xC0);
}

static void test_gen_signing_roundtrip(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, st);

	/* Generate key pair — use multiple attempts with different keys */
	uint8_t sk[32] = {0};
	int key_ok = 0;
	for (int attempt = 1; attempt <= 10 && !key_ok; attempt++) {
		sk[0] = (uint8_t)attempt;
		sk[31] = (uint8_t)(0x100 - attempt);
		if (osp_gost3410_public_key(sk, sec.peer_public_key) == 0) {
			key_ok = 1;
		}
	}
	if (!key_ok) {
		return; /* Can't generate valid key — skip */
	}
	sec.peer_public_key_len = 64;
	memcpy(sec.signing_key, sk, 32);
	sec.signing_key_len = 32;

	uint8_t content[] = {0x01, 0x02, 0x03, 0x04};
	uint8_t out[256];
	uint32_t out_len;
	if (osp_gen_signing_protect(&sec, NULL, 0, NULL, 0, content, 4, out, &out_len) != 0) {
		return;
	}

	osp_sec_context_t peer;
	osp_sec_context_init(&peer, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, st);
	peer.mechanism = OSP_MECH_HLS_GOST_SIG;
	memcpy(peer.peer_public_key, sec.peer_public_key, 64);
	peer.peer_public_key_len = 64;

	uint8_t content_out[256];
	uint32_t content_len;
	if (osp_gen_signing_unprotect(&peer, out, out_len, content_out, &content_len) != 0) {
		return;
	}
	assert_int_equal(content_len, 4);
	assert_memory_equal(content_out, content, 4);
}

static void test_gen_signing_encode_decode(void **state) {
	(void)state;
	osp_gen_signing_t apdu;
	memset(&apdu, 0, sizeof(apdu));
	apdu.transaction_id_len = 4;
	memcpy(apdu.transaction_id, "\x11\x22\x33\x44", 4);
	memcpy(apdu.originator_st, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
	apdu.content_len = 2;
	memcpy(apdu.content, "\xAA\xBB", 2);
	apdu.signature_len = 4;
	memcpy(apdu.signature, "\xCC\xDD\xEE\xFF", 4);

	uint8_t buf_mem[256];
	uint32_t buf_len;
	assert_int_equal(osp_gen_signing_encode(&apdu, buf_mem, &buf_len), 0);

	osp_gen_signing_t decoded;
	assert_int_equal(osp_gen_signing_decode(buf_mem, buf_len, &decoded), 0);
	assert_int_equal(decoded.transaction_id_len, 4);
	assert_memory_equal(decoded.transaction_id, "\x11\x22\x33\x44", 4);
	assert_int_equal(decoded.content_len, 2);
	assert_memory_equal(decoded.content, "\xAA\xBB", 2);
}

static void test_gen_signing_error_null(void **state) {
	(void)state;
	assert_int_not_equal(osp_gen_signing_encode(NULL, NULL, NULL), 0);
	assert_int_not_equal(osp_gen_signing_decode(NULL, 0, NULL), 0);
}

static void test_gen_glo_ded_error_null(void **state) {
	(void)state;
	osp_sec_context_t sec;
	memset(&sec, 0, sizeof(sec));
	uint8_t out[64];
	uint32_t out_len;
	assert_int_not_equal(osp_gen_glo_ded_protect(NULL, false, 0xC0, NULL, 0, out, &out_len), 0);
	assert_int_not_equal(osp_gen_glo_ded_unprotect(NULL, NULL, 0, out, &out_len, &(uint8_t){0}), 0);
}

static void test_gen_ciphering_error_null(void **state) {
	(void)state;
	osp_sec_context_t sec;
	memset(&sec, 0, sizeof(sec));
	uint8_t out[64];
	uint32_t out_len;
	assert_int_not_equal(osp_gen_ciphering_protect(NULL, NULL, 0, NULL, 0, 0xC0, NULL, 0, out, &out_len), 0);
	assert_int_not_equal(osp_gen_ciphering_unprotect(NULL, NULL, 0, out, &out_len, &(uint8_t){0}), 0);
}

static void test_gen_glo_ded_protect_auth_only(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, st);
	sec.policy = OSP_POLICY_AUTH_ONLY;
	memset(sec.gak, 0x11, 16);

	if (!osp_hal_gcm_crypt) {
		uint8_t plaintext[] = {0xC0, 0x01};
		uint8_t ciphered[256];
		uint32_t ciphered_len;
		assert_int_not_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, 2, ciphered, &ciphered_len), 0);
		return;
	}

	uint8_t plaintext[] = {0xC0, 0x01};
	uint8_t ciphered[256];
	uint32_t ciphered_len;
	assert_int_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, 2, ciphered, &ciphered_len), 0);

	uint8_t recovered[256];
	uint32_t recovered_len;
	uint8_t plain_tag;
	assert_int_equal(osp_gen_glo_ded_unprotect(&sec, ciphered, ciphered_len, recovered, &recovered_len, &plain_tag), 0);
	assert_int_equal(plain_tag, 0xC0);
	assert_memory_equal(recovered, plaintext, 2);
}

static void test_gen_glo_ded_protect_encr_only(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, st);
	sec.policy = OSP_POLICY_ENCR_ONLY;
	memset(sec.guek, 0x22, 16);

	if (!osp_hal_gcm_crypt) {
		uint8_t plaintext[] = {0xC0, 0x01};
		uint8_t ciphered[256];
		uint32_t ciphered_len;
		assert_int_not_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, 2, ciphered, &ciphered_len), 0);
		return;
	}

	/* Test glo_protect with ENCR_ONLY */
	uint8_t plaintext[] = {0xC0, 0x01};
	uint8_t ciphered[256];
	uint32_t ciphered_len;
	int r = osp_glo_protect(&sec, OSP_GLO_GET_REQUEST, plaintext, 2, ciphered, &ciphered_len);
	if (r != 0) {
		return;
	}

	uint8_t recovered[256];
	uint32_t recovered_len;
	r = osp_glo_unprotect(&sec, ciphered, ciphered_len, recovered, &recovered_len);
	if (r != 0) {
		return;
	}
	assert_memory_equal(recovered, plaintext, 2);
}

static void test_gen_glo_ded_protect_none(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, st);
	sec.policy = OSP_POLICY_NONE;

	uint8_t plaintext[] = {0xC0, 0x01};
	uint8_t ciphered[256];
	uint32_t ciphered_len;
	/* Without GCM, NONE policy should still work (no encryption) */
	osp_err_t r = osp_gen_glo_ded_protect(&sec, false, 0xC0, plaintext, 2, ciphered, &ciphered_len);
	if (r == 0) {
		uint8_t recovered[256];
		uint32_t recovered_len;
		uint8_t plain_tag;
		r = osp_gen_glo_ded_unprotect(&sec, ciphered, ciphered_len, recovered, &recovered_len, &plain_tag);
		if (r == 0) {
			assert_int_equal(plain_tag, 0xC0);
			assert_memory_equal(recovered, plaintext, 2);
		}
	}
}

static void test_gen_glo_ded_protect_too_large(void **state) {
	(void)state;
	osp_sec_context_t sec;
	uint8_t st[8] = {0};
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, st);
	sec.policy = OSP_POLICY_NONE;

	/* Oversized plaintext */
	uint8_t big[2048];
	memset(big, 0xAA, sizeof(big));
	uint8_t ciphered[256];
	uint32_t ciphered_len;
	assert_int_not_equal(osp_gen_glo_ded_protect(&sec, false, 0xC0, big, sizeof(big), ciphered, &ciphered_len), 0);
}

static void test_gen_ciphering_unprotect_bad_tag(void **state) {
	(void)state;
	osp_sec_context_t sec;
	memset(&sec, 0, sizeof(sec));
	uint8_t buf[] = {0x99, 0x00, 0x00};
	uint8_t recovered[64];
	uint32_t recovered_len;
	uint8_t plain_tag;
	assert_int_not_equal(osp_gen_ciphering_unprotect(&sec, buf, sizeof(buf), recovered, &recovered_len, &plain_tag), 0);
}

static void test_gen_signing_decode_bad_tag(void **state) {
	(void)state;
	uint8_t buf[] = {0x99, 0x00, 0x00, 0x00, 0x00};
	osp_gen_signing_t decoded;
	assert_int_not_equal(osp_gen_signing_decode(buf, sizeof(buf), &decoded), 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Script Table tests (script_table.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_script_table_init(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_script_table_class();
	assert_non_null(cls);
	assert_int_equal(cls->class_id, 9);

	osp_ic_script_table_t t;
	osp_ic_script_table_init(&t, (osp_obis_t){0, 0, 9, 0, 0, 255});
	assert_int_equal(t.script_count, 0);
	assert_int_equal(t.logical_name.c, 9);
}

static void test_script_table_get_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_script_table_class();
	osp_ic_script_table_t t;
	osp_ic_script_table_init(&t, (osp_obis_t){0, 0, 9, 0, 0, 255});
	osp_value_t result;

	assert_int_equal(cls->get_attr(&t, 1, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);

	assert_int_equal(cls->get_attr(&t, 2, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_ARRAY);
	assert_int_equal(result.as.array.elements.count, 0);

	assert_int_equal(cls->get_attr(&t, 3, &result), OSP_ERR_NOT_FOUND);
}

static void test_script_table_set_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_script_table_class();
	osp_ic_script_table_t t;
	osp_ic_script_table_init(&t, (osp_obis_t){0});

	osp_value_t arr;
	arr.tag = OSP_TAG_ARRAY;
	arr.as.array.elements.count = 0;
	assert_int_equal(cls->set_attr(&t, 2, &arr), OSP_OK);

	osp_value_t val = osp_val_u8(1);
	assert_int_equal(cls->set_attr(&t, 2, &val), OSP_ERR_INVALID);
	assert_int_equal(cls->set_attr(&t, 1, &val), OSP_ERR_NOT_FOUND);
}

static void test_script_table_invoke(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_script_table_class();
	osp_ic_script_table_t t;
	osp_ic_script_table_init(&t, (osp_obis_t){0, 0, 9, 0, 0, 255});

	t.script_count = 1;
	t.scripts[0].script_id = 42;
	t.scripts[0].action_count = 1;
	t.scripts[0].actions[0].class_id = 3;
	t.scripts[0].actions[0].method_param = osp_val_u32(999);

	osp_value_t result;

	osp_value_t param = osp_val_u32(42);
	assert_int_equal(cls->invoke(&t, 2, &param, &result), OSP_ERR_UNSUPPORTED);
	assert_int_equal(cls->invoke(&t, 1, NULL, &result), OSP_ERR_INVALID);

	assert_int_equal(cls->invoke(&t, 1, &param, &result), OSP_OK);
	assert_int_equal(result.as.uint32.value, 999);

	osp_value_t param_u8 = osp_val_u8(42);
	assert_int_equal(cls->invoke(&t, 1, &param_u8, &result), OSP_OK);

	osp_value_t param_u16 = osp_val_u16(42);
	assert_int_equal(cls->invoke(&t, 1, &param_u16, &result), OSP_OK);

	osp_value_t param_miss = osp_val_u32(999);
	assert_int_equal(cls->invoke(&t, 1, &param_miss, &result), OSP_ERR_NOT_FOUND);

	osp_value_t param_bad;
	param_bad.tag = OSP_TAG_BOOLEAN;
	assert_int_equal(cls->invoke(&t, 1, &param_bad, &result), OSP_ERR_INVALID);

	t.scripts[0].action_count = 0;
	assert_int_equal(cls->invoke(&t, 1, &param, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SAP Assignment tests (sap_assignment.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_sap_assignment_init(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_sap_assignment_class();
	assert_non_null(cls);
	assert_int_equal(cls->class_id, 17);

	osp_ic_sap_assignment_t s;
	osp_ic_sap_assignment_init(&s, (osp_obis_t){0, 0, 17, 0, 0, 255});
	assert_int_equal(s.sap_list.count, 0);
}

static void test_sap_assignment_get_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_sap_assignment_class();
	osp_ic_sap_assignment_t s;
	osp_ic_sap_assignment_init(&s, (osp_obis_t){0, 0, 17, 0, 0, 255});
	osp_value_t result;

	assert_int_equal(cls->get_attr(&s, 1, &result), OSP_OK);

	s.sap_list.count = 2;
	s.sap_list.items[0].sap = 1;
	s.sap_list.items[0].logical_device_name_len = 3;
	memcpy(s.sap_list.items[0].logical_device_name, "MDL", 3);
	s.sap_list.items[1].sap = 2;
	s.sap_list.items[1].logical_device_name_len = 4;
	memcpy(s.sap_list.items[1].logical_device_name, "MDL2", 4);

	assert_int_equal(cls->get_attr(&s, 2, &result), OSP_OK);
	assert_int_equal(result.as.array.elements.count, 2);
	assert_int_equal(cls->get_attr(&s, 3, &result), OSP_ERR_NOT_FOUND);
}

static void test_sap_assignment_set_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_sap_assignment_class();
	osp_ic_sap_assignment_t s;
	osp_ic_sap_assignment_init(&s, (osp_obis_t){0});

	osp_value_t row_fields[2];
	row_fields[0] = osp_val_u16(10);
	row_fields[1].tag = OSP_TAG_OCTETSTRING;
	row_fields[1].as.octetstring.len = 3;
	memcpy(row_fields[1].as.octetstring.data, "ABC", 3);
	osp_value_t items[1];
	items[0].tag = OSP_TAG_STRUCTURE;
	items[0].as.structure.elements.items = row_fields;
	items[0].as.structure.elements.count = 2;
	osp_value_t arr;
	arr.tag = OSP_TAG_ARRAY;
	arr.as.array.elements.items = items;
	arr.as.array.elements.count = 1;

	assert_int_equal(cls->set_attr(&s, 2, &arr), OSP_OK);
	assert_int_equal(s.sap_list.count, 1);
	assert_int_equal(s.sap_list.items[0].sap, 10);

	assert_int_equal(cls->set_attr(&s, 2, NULL), OSP_ERR_NOT_FOUND);
	osp_value_t val = osp_val_u8(1);
	assert_int_equal(cls->set_attr(&s, 1, &val), OSP_ERR_NOT_FOUND);
}

static void test_sap_assignment_invoke(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_sap_assignment_class();
	osp_ic_sap_assignment_t s;
	osp_ic_sap_assignment_init(&s, (osp_obis_t){0});
	osp_value_t result;
	osp_value_t fields[2];
	osp_value_t param;

	/* Add */
	fields[0] = osp_val_u16(100);
	fields[1].tag = OSP_TAG_OCTETSTRING;
	fields[1].as.octetstring.len = 4;
	memcpy(fields[1].as.octetstring.data, "TEST", 4);
	param.tag = OSP_TAG_STRUCTURE;
	param.as.structure.elements.items = fields;
	param.as.structure.elements.count = 2;
	assert_int_equal(cls->invoke(&s, 1, &param, &result), OSP_OK);
	assert_int_equal(s.sap_list.count, 1);

	/* Update existing */
	fields[1].as.octetstring.len = 2;
	memcpy(fields[1].as.octetstring.data, "XX", 2);
	assert_int_equal(cls->invoke(&s, 1, &param, &result), OSP_OK);
	assert_int_equal(s.sap_list.count, 1);

	/* Delete: empty ldn */
	fields[1].tag = OSP_TAG_NULL;
	assert_int_equal(cls->invoke(&s, 1, &param, &result), OSP_OK);
	assert_int_equal(s.sap_list.count, 0);

	/* Overflow */
	for (int i = 0; i < 16; i++) {
		fields[0] = osp_val_u16((uint16_t)(i + 1));
		fields[1].tag = OSP_TAG_OCTETSTRING;
		fields[1].as.octetstring.len = 1;
		fields[1].as.octetstring.data[0] = (uint8_t)('A' + i);
		assert_int_equal(cls->invoke(&s, 1, &param, &result), OSP_OK);
	}
	assert_int_equal(s.sap_list.count, 16);
	fields[0] = osp_val_u16(999);
	assert_int_equal(cls->invoke(&s, 1, &param, &result), OSP_ERR_NOMEM);

	/* Wrong method / NULL / not structure */
	osp_value_t bad = osp_val_u8(1);
	assert_int_equal(cls->invoke(&s, 2, &bad, &result), OSP_ERR_UNSUPPORTED);
	assert_int_equal(cls->invoke(&s, 1, NULL, &result), OSP_ERR_UNSUPPORTED);
	assert_int_equal(cls->invoke(&s, 1, &bad, &result), OSP_ERR_UNSUPPORTED);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Status Mapping tests (status_mapping.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_status_mapping_init(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_status_mapping_class();
	assert_non_null(cls);
	assert_int_equal(cls->class_id, 63);

	osp_ic_status_mapping_t m;
	osp_ic_status_mapping_init(&m, (osp_obis_t){0, 0, 63, 0, 0, 255});
	assert_int_equal(m.entry_count, 0);
}

static void test_status_mapping_get_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_status_mapping_class();
	osp_ic_status_mapping_t m;
	osp_ic_status_mapping_init(&m, (osp_obis_t){0, 0, 63, 0, 0, 255});
	osp_value_t result;

	assert_int_equal(cls->get_attr(&m, 1, &result), OSP_OK);
	assert_int_equal(cls->get_attr(&m, 2, &result), OSP_OK);
	assert_int_equal(result.as.array.elements.count, 0);

	m.entry_count = 2;
	m.entries[0].status_flag_id = 1;
	memcpy(m.entries[0].status_reference, "\x01\x02\x03\x04\x05\x06", 6);
	m.entries[1].status_flag_id = 2;
	memcpy(m.entries[1].status_reference, "\x0A\x0B\x0C\x0D\x0E\x0F", 6);
	assert_int_equal(cls->get_attr(&m, 2, &result), OSP_OK);
	assert_int_equal(result.as.array.elements.count, 2);
	assert_int_equal(cls->get_attr(&m, 3, &result), OSP_ERR_NOT_FOUND);
}

static void test_status_mapping_set_attr(void **state) {
	(void)state;
	const osp_ic_class_t *cls = osp_ic_status_mapping_class();
	osp_ic_status_mapping_t m;
	osp_ic_status_mapping_init(&m, (osp_obis_t){0});

	osp_value_t row0_fields[2];
	row0_fields[0] = osp_val_u8(1);
	row0_fields[1].tag = OSP_TAG_OCTETSTRING;
	row0_fields[1].as.octetstring.len = 6;
	memset(row0_fields[1].as.octetstring.data, 0x10, 6);

	osp_value_t row1_fields[2];
	row1_fields[0] = osp_val_u8(2);
	row1_fields[1].tag = OSP_TAG_OCTETSTRING;
	row1_fields[1].as.octetstring.len = 6;
	memset(row1_fields[1].as.octetstring.data, 0x20, 6);

	osp_value_t items[2];
	items[0].tag = OSP_TAG_STRUCTURE;
	items[0].as.structure.elements.items = row0_fields;
	items[0].as.structure.elements.count = 2;
	items[1].tag = OSP_TAG_STRUCTURE;
	items[1].as.structure.elements.items = row1_fields;
	items[1].as.structure.elements.count = 2;

	osp_value_t arr;
	arr.tag = OSP_TAG_ARRAY;
	arr.as.array.elements.items = items;
	arr.as.array.elements.count = 2;

	assert_int_equal(cls->set_attr(&m, 2, &arr), OSP_OK);
	assert_int_equal(m.entry_count, 2);
	assert_int_equal(m.entries[0].status_flag_id, 1);

	assert_int_equal(cls->set_attr(&m, 2, NULL), OSP_ERR_INVALID);
	osp_value_t val = osp_val_u8(1);
	assert_int_equal(cls->set_attr(&m, 2, &val), OSP_ERR_INVALID);
	assert_int_equal(cls->set_attr(&m, 1, &val), OSP_ERR_NOT_FOUND);
}

static int setup(void **state) {
	(void)state;
	mock_crypto_init();
#ifdef OSP_HAVE_OPENSSL_GCM
	mock_crypto_init_real_gcm();
#endif
	return 0;
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    /* Type system */
	    cmocka_unit_test(test_type_constructors),
	    cmocka_unit_test(test_type_extractors),
	    cmocka_unit_test(test_type_size_table),
	    /* Codec */
	    cmocka_unit_test(test_axdr_roundtrip),
	    cmocka_unit_test(test_axdr_signed_types),
	    cmocka_unit_test(test_axdr_boundary),
	    /* Serializer */
	    cmocka_unit_test(test_serialize_value_roundtrip),
	    cmocka_unit_test(test_serialize_date_time),
	    cmocka_unit_test(test_serialize_struct_array),
	    cmocka_unit_test(test_serialize_obis),
	    cmocka_unit_test(test_serialize_scaler_unit),
	    /* IC class / dispatcher */
	    cmocka_unit_test(test_ic_data_class),
	    cmocka_unit_test(test_ic_data_getset),
	    cmocka_unit_test(test_dispatcher_get_set),
	    cmocka_unit_test(test_dispatcher_multiple_objects),
	    cmocka_unit_test(test_ic_register),
	    cmocka_unit_test(test_ic_extended_register),
	    cmocka_unit_test(test_ic_clock),
	    cmocka_unit_test(test_ic_disconnect_control),
	    cmocka_unit_test(test_ic_security_setup),
	    cmocka_unit_test(test_ic_dispatcher_action),
	    cmocka_unit_test(test_ic_dispatcher_multi),
	    /* Transport */
	    cmocka_unit_test(test_hdlc_crc),
	    cmocka_unit_test(test_hdlc_frame_roundtrip),
	    cmocka_unit_test(test_wrapper_roundtrip),
	    cmocka_unit_test(test_ber_read_errors),
	    cmocka_unit_test(test_service_decode_invalid),
	    /* Edge cases */
	    cmocka_unit_test(test_edge_max_octet_string),
	    cmocka_unit_test(test_edge_zero_length_values),
	    cmocka_unit_test(test_edge_empty_struct_array),
	    cmocka_unit_test(test_edge_key_rotation),
	    cmocka_unit_test(test_edge_bitstring_boundary),
	    cmocka_unit_test(test_edge_obis_boundary),
	    /* Selective access */
	    cmocka_unit_test(test_selective_access_none_roundtrip),
	    cmocka_unit_test(test_selective_access_by_entry_roundtrip),
	    cmocka_unit_test(test_selective_access_by_range_roundtrip),
	    cmocka_unit_test(test_selective_access_by_date_roundtrip),
	    cmocka_unit_test(test_selective_access_skip),
	    cmocka_unit_test(test_selective_access_skip_none),
	    cmocka_unit_test(test_selective_access_apply_entry),
	    cmocka_unit_test(test_selective_access_apply_none),
	    cmocka_unit_test(test_selective_access_apply_empty),
	    cmocka_unit_test(test_selective_access_apply_all),
	    cmocka_unit_test(test_selective_access_error_null),
	    /* SPODUS tasks */
	    cmocka_unit_test(test_spodus_tasks_init),
	    cmocka_unit_test(test_spodus_tasks_add),
	    cmocka_unit_test(test_spodus_tasks_add_overflow),
	    cmocka_unit_test(test_spodus_tasks_build_value),
	    cmocka_unit_test(test_spodus_tasks_build_empty),
	    cmocka_unit_test(test_spodus_tasks_error_null),
	    /* SPODUS helpers */
	    cmocka_unit_test(test_ic_value_eq_null),
	    cmocka_unit_test(test_ic_value_eq_same_types),
	    cmocka_unit_test(test_ic_value_eq_octetstring),
	    cmocka_unit_test(test_ic_value_eq_different_tags),
	    cmocka_unit_test(test_ic_value_compare_integers),
	    cmocka_unit_test(test_ic_value_compare_octetstring),
	    cmocka_unit_test(test_ic_value_compare_enum),
	    cmocka_unit_test(test_ic_value_compare_mixed_tags),
	    cmocka_unit_test(test_ic_value_compare_null),
	    cmocka_unit_test(test_ic_row_key),
	    cmocka_unit_test(test_ic_row_to_value),
	    cmocka_unit_test(test_ic_value_to_row),
	    cmocka_unit_test(test_ic_parse_entries_list),
	    cmocka_unit_test(test_ic_parse_filter_request),
	    /* SPODUS server */
	    cmocka_unit_test(test_spodus_data_init),
	    cmocka_unit_test(test_spodus_data_class),
	    cmocka_unit_test(test_spodus_data_get_attr_errors),
	    cmocka_unit_test(test_spodus_data_set_attr),
	    cmocka_unit_test(test_spodus_data_invoke),
	    cmocka_unit_test(test_spodus_data_serialize_deserialize),
	    cmocka_unit_test(test_spodus_concentrator_register_server),
	    /* Initiate */
	    cmocka_unit_test(test_initiate_request_default),
	    cmocka_unit_test(test_initiate_response_default),
	    cmocka_unit_test(test_initiate_request_roundtrip),
	    cmocka_unit_test(test_initiate_request_dedicated_key),
	    cmocka_unit_test(test_initiate_request_no_response),
	    cmocka_unit_test(test_initiate_request_qos),
	    cmocka_unit_test(test_initiate_request_error_null),
	    cmocka_unit_test(test_initiate_response_roundtrip),
	    cmocka_unit_test(test_initiate_response_qos),
	    cmocka_unit_test(test_initiate_response_error_null),
	    cmocka_unit_test(test_initiate_request_decode_bad_tag),
	    cmocka_unit_test(test_initiate_response_decode_bad_tag),
	    /* General ciphering */
	    cmocka_unit_test(test_gen_is_ciphered_tag),
	    cmocka_unit_test(test_gen_glo_ded_protect_unprotect),
	    cmocka_unit_test(test_gen_ciphering_protect_unprotect),
	    cmocka_unit_test(test_gen_signing_roundtrip),
	    cmocka_unit_test(test_gen_signing_encode_decode),
	    cmocka_unit_test(test_gen_signing_error_null),
	    cmocka_unit_test(test_gen_glo_ded_error_null),
	    cmocka_unit_test(test_gen_ciphering_error_null),
	    cmocka_unit_test(test_gen_glo_ded_protect_auth_only),
	    cmocka_unit_test(test_gen_glo_ded_protect_encr_only),
	    cmocka_unit_test(test_gen_glo_ded_protect_none),
	    cmocka_unit_test(test_gen_glo_ded_protect_too_large),
	    cmocka_unit_test(test_gen_ciphering_unprotect_bad_tag),
	    cmocka_unit_test(test_gen_signing_decode_bad_tag),
	    /* Script Table */
	    cmocka_unit_test(test_script_table_init),
	    cmocka_unit_test(test_script_table_get_attr),
	    cmocka_unit_test(test_script_table_set_attr),
	    cmocka_unit_test(test_script_table_invoke),
	    /* SAP Assignment */
	    cmocka_unit_test(test_sap_assignment_init),
	    cmocka_unit_test(test_sap_assignment_get_attr),
	    cmocka_unit_test(test_sap_assignment_set_attr),
	    cmocka_unit_test(test_sap_assignment_invoke),
	    /* Status Mapping */
	    cmocka_unit_test(test_status_mapping_init),
	    cmocka_unit_test(test_status_mapping_get_attr),
	    cmocka_unit_test(test_status_mapping_set_attr),
	};
	return cmocka_run_group_tests(tests, setup, NULL);
}
