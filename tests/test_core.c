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
#include "server/dispatcher.h"
#include "ic/data.h"
#include "ic/register.h"
#include "ic/extended_register.h"
#include "ic/clock.h"
#include "ic/disconnect_control.h"
#include "ic/security_setup.h"
#include "transport/transport.h"

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
	assert_int_equal(v.tag, OSP_TAG_DATETIME);

	/* Set time */
	osp_value_t new_time = osp_val_datetime(2026, 7, 9, 3, 14, 30, 0, 0);
	assert_int_equal(cls->set_attr(&clock, 2, &new_time), OSP_OK);
	assert_int_equal(cls->get_attr(&clock, 2, &v), OSP_OK);
	assert_int_equal(v.as.datetime.date.year, 2026);

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
	assert_int_equal(v.tag, OSP_TAG_DATETIME);
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
	assert_int_equal(osp_get_u8(&v), 1);

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
 *  Test runner
 * ═══════════════════════════════════════════════════════════════════════════ */

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
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
