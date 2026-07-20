/**
 * test_phase0.c — Phase 0 parity tests (initiate, ic_serialize, SPODUS IC, ACL)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "codec/ic_serialize.h"
#include "service/initiate.h"
#include "server/dispatcher.h"
#include "ic/data.h"
#include "ic/table_manager.h"
#include "ic/profile_data_filter.h"
#include "ic/mbus_slave_port_setup.h"
#include "ic/association_ln.h"

static osp_value_t val_octet(const uint8_t *d, uint8_t len) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	memcpy(v.as.octetstring.data, d, len);
	v.as.octetstring.len = len;
	return v;
}

static osp_value_t make_row(osp_value_t *fields, uint8_t n) {
	osp_value_t row = {0};
	row.tag = OSP_TAG_STRUCTURE;
	row.as.structure.elements.items = fields;
	row.as.structure.elements.count = n;
	row.as.structure.elements.capacity = n;
	return row;
}

static osp_value_t make_wrap(osp_value_t *entries, uint8_t count) {
	static osp_value_t wrap_fields[2];
	static osp_value_t wrap;
	wrap_fields[0] = osp_val_null();
	wrap_fields[1].tag = OSP_TAG_ARRAY;
	wrap_fields[1].as.array.elements.items = entries;
	wrap_fields[1].as.array.elements.count = count;
	wrap_fields[1].as.array.elements.capacity = count;
	wrap.tag = OSP_TAG_STRUCTURE;
	wrap.as.structure.elements.items = wrap_fields;
	wrap.as.structure.elements.count = 2;
	wrap.as.structure.elements.capacity = 2;
	return wrap;
}

static void test_initiate_request_green_book(void **state) {
	(void)state;
	const uint8_t expected[] = {0x01, 0x00, 0x00, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x7E, 0x1F, 0x04, 0xB0};
	uint8_t mem[32];
	osp_buf_t buf;
	osp_buf_init(&buf, mem, sizeof(mem));

	osp_initiate_request_t req;
	osp_initiate_request_default(&req);
	assert_int_equal(osp_initiate_request_encode(&req, &buf), OSP_OK);
	assert_int_equal(buf.wr, sizeof(expected));
	assert_memory_equal(mem, expected, sizeof(expected));

	osp_initiate_request_t decoded;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, mem, buf.wr);
	rbuf.wr = buf.wr;
	assert_int_equal(osp_initiate_request_decode(&rbuf, &decoded), OSP_OK);
	assert_int_equal(decoded.proposed_dlms_version, 6);
	assert_int_equal(decoded.proposed_conformance, 0x007E1F);
	assert_int_equal(decoded.client_max_receive_pdu_size, 0x04B0);
}

static void test_initiate_response_green_book(void **state) {
	(void)state;
	const uint8_t expected[] = {0x08, 0x00, 0x06, 0x5F, 0x1F, 0x04, 0x00, 0x00, 0x7E, 0x1F, 0x08, 0x00, 0x00, 0x07};
	uint8_t mem[32];
	osp_buf_t buf;
	osp_buf_init(&buf, mem, sizeof(mem));

	osp_initiate_response_t resp;
	osp_initiate_response_default(&resp);
	assert_int_equal(osp_initiate_response_encode(&resp, &buf), OSP_OK);
	assert_int_equal(buf.wr, sizeof(expected));
	assert_memory_equal(mem, expected, sizeof(expected));
}

static void test_ic_data_serialize_roundtrip(void **state) {
	(void)state;
	osp_ic_data_t data;
	osp_ic_data_init(&data, (osp_obis_t){0, 0, 0x80, 0, 0, 0xFF});
	data.value = osp_val_u16(0x1234);

	uint8_t mem[64];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_ic_serialize(osp_ic_data_class(), &data, &w), OSP_OK);
	assert_true(w.wr > 0);

	osp_ic_data_t copy;
	osp_ic_data_init(&copy, (osp_obis_t){0, 0, 0, 0, 0, 0});
	osp_buf_t r;
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	assert_int_equal(osp_ic_deserialize(osp_ic_data_class(), &copy, &r), OSP_OK);
	assert_true(osp_obis_eq(&copy.logical_name, &data.logical_name));
	assert_int_equal(osp_get_u16(&copy.value), 0x1234);
}

static void test_table_manager_group_ops(void **state) {
	(void)state;
	osp_ic_table_manager_t mgr;
	osp_ic_table_manager_init(&mgr, (osp_obis_t){0, 0, 94, 7, 200, 255});
	osp_ic_table_manager_set_key_index(&mgr, 0);

	osp_value_t row_a_fields[2] = {val_octet((const uint8_t *)"A", 1), osp_val_u16(1)};
	osp_value_t row_b_fields[2] = {val_octet((const uint8_t *)"B", 1), osp_val_u16(2)};
	osp_value_t entries[2] = {make_row(row_a_fields, 2), make_row(row_b_fields, 2)};
	osp_value_t wrap = make_wrap(entries, 2);

	const osp_ic_class_t *cls = osp_ic_table_manager_class();
	osp_value_t result;
	assert_int_equal(cls->invoke(&mgr, 1, &wrap, &result), OSP_OK);
	assert_int_equal(cls->invoke(&mgr, 3, NULL, &result), OSP_OK);
	assert_int_equal(osp_get_u8(&result), 2);

	osp_value_t upd_fields[2] = {val_octet((const uint8_t *)"A", 1), osp_val_u16(100)};
	osp_value_t upd[1] = {make_row(upd_fields, 2)};
	wrap = make_wrap(upd, 1);
	assert_int_equal(cls->invoke(&mgr, 1, &wrap, &result), OSP_OK);

	osp_value_t key_a[1] = {val_octet((const uint8_t *)"A", 1)};
	wrap = make_wrap(key_a, 1);
	assert_int_equal(cls->invoke(&mgr, 4, &wrap, &result), OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_ARRAY);
	assert_int_equal(result.as.array.elements.count, 1);
	assert_int_equal(osp_get_u16(&result.as.array.elements.items[0].as.structure.elements.items[1]), 100);
}

static void test_profile_data_filter_range(void **state) {
	(void)state;
	osp_obis_t cols[2] = {{0, 0, 94, 7, 128, 10}, {1, 0, 1, 8, 0, 255}};
	osp_ic_profile_data_filter_t filter;
	osp_ic_profile_data_filter_init(&filter, (osp_obis_t){0, 0, 94, 7, 201, 255});
	osp_ic_profile_data_filter_set_columns(&filter, cols, 2);

	osp_ic_row_t rows[3];
	osp_value_t ra[2] = {val_octet((const uint8_t *)"A", 1), osp_val_u16(100)};
	osp_value_t rb[2] = {val_octet((const uint8_t *)"B", 1), osp_val_u16(200)};
	osp_value_t rc[2] = {val_octet((const uint8_t *)"C", 1), osp_val_u16(300)};
	memcpy(rows[0].fields, ra, sizeof(ra));
	rows[0].field_count = 2;
	memcpy(rows[1].fields, rb, sizeof(rb));
	rows[1].field_count = 2;
	memcpy(rows[2].fields, rc, sizeof(rc));
	rows[2].field_count = 2;
	osp_ic_profile_data_filter_set_rows(&filter, rows, 3);

	osp_value_t col_def_fields[4] = {osp_val_u16(1), val_octet((const uint8_t *)&cols[1], 6), osp_val_i8(2), osp_val_u16(0)};
	osp_value_t col_def = make_row(col_def_fields, 4);
	osp_value_t filter_fields[4] = {col_def, osp_val_u16(150), osp_val_u16(250), osp_val_null()};
	osp_value_t filter_one = make_row(filter_fields, 4);
	osp_value_t filters[1] = {filter_one};
	osp_value_t wrap_fields[3];
	wrap_fields[0] = osp_val_null();
	wrap_fields[1].tag = OSP_TAG_ARRAY;
	wrap_fields[1].as.array.elements.count = 0;
	wrap_fields[2].tag = OSP_TAG_ARRAY;
	wrap_fields[2].as.array.elements.items = filters;
	wrap_fields[2].as.array.elements.count = 1;
	osp_value_t param = make_row(wrap_fields, 3);

	osp_value_t result;
	const osp_ic_class_t *cls = osp_ic_profile_data_filter_class();
	assert_int_equal(cls->invoke(&filter, 1, &param, &result), OSP_OK);
	assert_int_equal(osp_get_u8(&result), 1);
	assert_int_equal(cls->invoke(&filter, 2, &param, &result), OSP_OK);
	assert_int_equal(result.as.array.elements.count, 1);
}

static void test_mbus_slave_port_setup_class25(void **state) {
	(void)state;
	osp_ic_mbus_slave_port_setup_t mbus;
	osp_ic_mbus_slave_port_setup_init(&mbus, (osp_obis_t){0, 0, 24, 1, 0, 255});
	mbus.default_baud = 5;
	mbus.bus_address = 17;

	const osp_ic_class_t *cls = osp_ic_mbus_slave_port_setup_class();
	assert_int_equal(cls->class_id, 25);

	osp_value_t v;
	assert_int_equal(cls->get_attr(&mbus, 5, &v), OSP_OK);
	assert_int_equal(osp_get_u8(&v), 17);
}

static void test_dispatcher_acl(void **state) {
	(void)state;
	osp_ic_data_t data;
	osp_ic_data_init(&data, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data.value = osp_val_u32(42);

	osp_ic_association_ln_t aln;
	osp_ic_association_ln_init(&aln, (osp_obis_t){0, 0, 40, 0, 0, 255});
	osp_object_list_element_t elem = {.class_id = 1, .logical_name = data.logical_name};
	elem.access_rights.attr_count = 1;
	elem.access_rights.attr_items[0].attribute_id = 1;
	elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_READ_ONLY;
	assert_int_equal(osp_ic_association_ln_add_object(&aln, &elem), OSP_OK);

	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);
	osp_dispatcher_set_association(&disp, &aln);
	assert_int_equal(osp_dispatcher_register(&disp, osp_ic_data_class(), &data), OSP_OK);

	osp_value_t v;
	assert_int_equal(osp_dispatcher_get(&disp, 1, &data.logical_name, 1, &v), OSP_OK);
	assert_int_equal(osp_dispatcher_set(&disp, 1, &data.logical_name, 1, &v), OSP_ERR_SECURITY);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_initiate_request_green_book),
	    cmocka_unit_test(test_initiate_response_green_book),
	    cmocka_unit_test(test_ic_data_serialize_roundtrip),
	    cmocka_unit_test(test_table_manager_group_ops),
	    cmocka_unit_test(test_profile_data_filter_range),
	    cmocka_unit_test(test_mbus_slave_port_setup_class25),
	    cmocka_unit_test(test_dispatcher_acl),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
