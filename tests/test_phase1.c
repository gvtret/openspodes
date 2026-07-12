/**
 * test_phase1.c — Phase 1 IC parity tests (fixtures + golden vectors + round-trips)
 *
 * Fixture attribute values:
 *   - doc-rag-remote: Blue Book §4.3.x, STO 34.01-5.1-006, GOST R 58940-2020
 *   - spodes-rs/tests/integration.rs (parity)
 * Golden BER bytes: tests/fixtures/ic_golden_vectors.h (regen via gen_ic_golden_vectors)
 * Codec primitives: docs/golden_vectors.txt (covered in test_codec_golden.c)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#include "openspodes.h"
#include "codec/ic_serialize.h"
#include "codec/serialize.h"
#include "fixtures/ic_spodes_fixtures.h"
#include "fixtures/ic_golden_vectors.h"

static void assert_ic_roundtrip(const char *name, const osp_ic_class_t *cls, void *orig, void *copy, size_t size) {
	uint8_t mem[4096];
	osp_buf_t w, r;
	osp_err_t err;

	osp_buf_init(&w, mem, sizeof(mem));
	assert_non_null(cls->serialize);
	assert_non_null(cls->deserialize);
	err = osp_ic_serialize(cls, orig, &w);
	if (err != OSP_OK) {
		fprintf(stderr, "serialize failed for %s (%s): %d\n", name, cls->name, err);
	}
	assert_int_equal(err, OSP_OK);
	memset(copy, 0, size);
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	err = osp_ic_deserialize(cls, copy, &r);
	if (err != OSP_OK) {
		fprintf(stderr, "deserialize failed for %s (%s): %d size=%u\n", name, cls->name, err, w.wr);
	}
	assert_int_equal(err, OSP_OK);
}

static void assert_serialize_matches_golden(const char *name, const osp_ic_class_t *cls, void *inst, const uint8_t *golden,
                                            size_t golden_len) {
	uint8_t mem[4096];
	osp_buf_t w;

	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_ic_serialize(cls, inst, &w), OSP_OK);
	if (w.wr != golden_len || memcmp(mem, golden, golden_len) != 0) {
		fprintf(stderr, "golden serialize mismatch for %s: got %u bytes, expected %zu\n", name, w.wr, golden_len);
	}
	assert_int_equal(w.wr, golden_len);
	assert_memory_equal(mem, golden, golden_len);
}

static void assert_golden_deserialize_roundtrip(const char *name, const osp_ic_class_t *cls, void *copy, size_t size,
                                                const uint8_t *golden, size_t golden_len) {
	uint8_t mem[4096];
	osp_buf_t r, w;

	memset(copy, 0, size);
	osp_buf_init(&r, (uint8_t *)golden, golden_len);
	r.wr = golden_len;
	assert_int_equal(osp_ic_deserialize(cls, copy, &r), OSP_OK);

	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_ic_serialize(cls, copy, &w), OSP_OK);
	assert_int_equal(w.wr, golden_len);
	if (memcmp(mem, golden, golden_len) != 0) {
		fprintf(stderr, "golden deserialize roundtrip mismatch for %s\n", name);
	}
	assert_memory_equal(mem, golden, golden_len);
}

typedef struct {
	const char *name;
	const osp_ic_class_t *(*class_fn)(void);
	size_t inst_size;
	void (*init)(void *);
} osp_ic_fixture_entry_t;

static const osp_ic_fixture_entry_t fixture_table[] = {
    {"data", osp_ic_data_class, sizeof(osp_ic_data_t), osp_fixture_data},
    {"register", osp_ic_register_class, sizeof(osp_ic_register_t), osp_fixture_register},
    {"extended_register", osp_ic_ext_register_class, sizeof(osp_ic_ext_register_t), osp_fixture_extended_register},
    {"demand_register", osp_ic_demand_register_class, sizeof(osp_ic_demand_register_t), osp_fixture_demand_register},
    {"clock", osp_ic_clock_class, sizeof(osp_ic_clock_t), osp_fixture_clock},
    {"profile_generic", osp_ic_profile_generic_class, sizeof(osp_ic_profile_generic_t), osp_fixture_profile_generic},
    {"disconnect_control", osp_ic_disconnect_control_class, sizeof(osp_ic_disconnect_control_t), osp_fixture_disconnect_control},
    {"security_setup", osp_ic_security_setup_class, sizeof(osp_ic_security_setup_t), osp_fixture_security_setup},
    {"limiter", osp_ic_limiter_class, sizeof(osp_ic_limiter_t), osp_fixture_limiter},
    {"schedule", osp_ic_schedule_class, sizeof(osp_ic_schedule_t), osp_fixture_schedule},
    {"script_table", osp_ic_script_table_class, sizeof(osp_ic_script_table_t), osp_fixture_script_table},
    {"special_days", osp_ic_special_days_class, sizeof(osp_ic_special_days_t), osp_fixture_special_days},
    {"register_activation", osp_ic_register_activation_class, sizeof(osp_ic_register_activation_t), osp_fixture_register_activation},
    {"sap_assignment", osp_ic_sap_assignment_class, sizeof(osp_ic_sap_assignment_t), osp_fixture_sap_assignment},
    {"image_transfer", osp_ic_image_transfer_class, sizeof(osp_ic_image_transfer_t), osp_fixture_image_transfer},
    {"activity_calendar", osp_ic_activity_calendar_class, sizeof(osp_ic_activity_calendar_t), osp_fixture_activity_calendar},
    {"register_monitor", osp_ic_register_monitor_class, sizeof(osp_ic_register_monitor_t), osp_fixture_register_monitor},
    {"single_action_schedule", osp_ic_single_action_schedule_class, sizeof(osp_ic_single_action_schedule_t),
     osp_fixture_single_action_schedule},
    {"ipv4_setup", osp_ic_ipv4_setup_class, sizeof(osp_ic_ipv4_setup_t), osp_fixture_ipv4_setup},
    {"tcp_udp_setup", osp_ic_tcp_udp_setup_class, sizeof(osp_ic_tcp_udp_setup_t), osp_fixture_tcp_udp_setup},
    {"arbitrator", osp_ic_arbitrator_class, sizeof(osp_ic_arbitrator_t), osp_fixture_arbitrator},
    {"iec_hdlc_setup", osp_ic_iec_hdlc_setup_class, sizeof(osp_ic_iec_hdlc_setup_t), osp_fixture_iec_hdlc_setup},
    {"iec_local_port_setup", osp_ic_iec_local_port_setup_class, sizeof(osp_ic_iec_local_port_setup_t), osp_fixture_iec_local_port_setup},
    {"data_protection", osp_ic_data_protection_class, sizeof(osp_ic_data_protection_t), osp_fixture_data_protection},
    {"push_setup", osp_ic_push_setup_class, sizeof(osp_ic_push_setup_t), osp_fixture_push_setup},
    {"mac_address", osp_ic_mac_address_class, sizeof(osp_ic_mac_address_t), osp_fixture_mac_address},
    {"gprs_modem", osp_ic_gprs_modem_class, sizeof(osp_ic_gprs_modem_t), osp_fixture_gprs_modem},
    {"gsm_diagnostic", osp_ic_gsm_diagnostic_class, sizeof(osp_ic_gsm_diagnostic_t), osp_fixture_gsm_diagnostic},
    {"ipv6_setup", osp_ic_ipv6_setup_class, sizeof(osp_ic_ipv6_setup_t), osp_fixture_ipv6_setup},
    {"mbus_slave_port_setup", osp_ic_mbus_slave_port_setup_class, sizeof(osp_ic_mbus_slave_port_setup_t), osp_fixture_mbus_slave_port_setup},
    {"profile_data_filter", osp_ic_profile_data_filter_class, sizeof(osp_ic_profile_data_filter_t), osp_fixture_profile_data_filter},
    {"table_manager", osp_ic_table_manager_class, sizeof(osp_ic_table_manager_t), osp_fixture_table_manager},
    {"register_table", osp_ic_register_table_class, sizeof(osp_ic_register_table_t), osp_fixture_register_table},
    {"parameter_monitor", osp_ic_parameter_monitor_class, sizeof(osp_ic_parameter_monitor_t), osp_fixture_parameter_monitor},
    {"mbus_slave", osp_ic_mbus_slave_class, sizeof(osp_ic_mbus_slave_t), osp_fixture_mbus_slave},
    {"utility_tables", osp_ic_utility_tables_class, sizeof(osp_ic_utility_tables_t), osp_fixture_utility_tables},
    {"status_mapping", osp_ic_status_mapping_class, sizeof(osp_ic_status_mapping_t), osp_fixture_status_mapping},
    {"compact_data", osp_ic_compact_data_class, sizeof(osp_ic_compact_data_t), osp_fixture_compact_data},
    {"profile_filter", osp_ic_profile_filter_class, sizeof(osp_ic_profile_filter_t), osp_fixture_profile_filter},
    {"association_ln", osp_ic_association_ln_class, sizeof(osp_ic_association_ln_t), osp_fixture_association_ln},
};

static const osp_ic_fixture_entry_t *fixture_lookup(const char *name) {
	for (size_t i = 0; i < sizeof(fixture_table) / sizeof(fixture_table[0]); i++) {
		if (strcmp(fixture_table[i].name, name) == 0) {
			return &fixture_table[i];
		}
	}
	return NULL;
}

#define IC_FIXTURE_ROUNDTRIP(label, entry)                          \
	do {                                                       \
		void *orig = calloc(1, (entry)->inst_size);         \
		void *copy = calloc(1, (entry)->inst_size);         \
		assert_non_null(orig);                               \
		assert_non_null(copy);                               \
		(entry)->init(orig);                                 \
		assert_ic_roundtrip(label, (entry)->class_fn(), orig, copy, (entry)->inst_size); \
		free(orig);                                          \
		free(copy);                                          \
	} while (0)

static void test_ic_spodes_golden_vectors(void **state) {
	(void)state;

	for (size_t i = 0; i < osp_ic_golden_vector_count; i++) {
		const osp_ic_golden_vector_t *gv = &osp_ic_golden_vectors[i];
		const osp_ic_fixture_entry_t *fe = fixture_lookup(gv->name);
		void *inst = NULL;
		void *copy = NULL;

		assert_non_null(fe);
		assert_int_equal(fe->class_fn()->class_id, gv->class_id);

		inst = calloc(1, fe->inst_size);
		copy = calloc(1, fe->inst_size);
		assert_non_null(inst);
		assert_non_null(copy);
		fe->init(inst);

		assert_serialize_matches_golden(gv->name, fe->class_fn(), inst, gv->bytes, gv->len);
		assert_golden_deserialize_roundtrip(gv->name, fe->class_fn(), copy, fe->inst_size, gv->bytes, gv->len);

		free(inst);
		free(copy);
	}
}

/* integration.rs field checks after golden deserialize */
static void test_ic_bluebook_register_table5(void **state) {
	(void)state;
	osp_ic_register_t orig, copy;
	osp_fixture_register_bluebook_kwh(&orig);

	uint8_t mem[256];
	osp_buf_t w, r;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_ic_serialize(osp_ic_register_class(), &orig, &w), OSP_OK);

	osp_ic_register_init(&copy, (osp_obis_t){0, 0, 0, 0, 0, 0}, osp_val_null());
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	assert_int_equal(osp_ic_deserialize(osp_ic_register_class(), &copy, &r), OSP_OK);

	assert_int_equal(osp_get_u32(&copy.value), 593);
	assert_int_equal(copy.scaler_unit.scaler, 3);
	assert_int_equal(copy.scaler_unit.unit, 30);
}

static void test_ic_golden_field_values(void **state) {
	(void)state;
	osp_ic_data_t data;
	osp_ic_register_t reg;
	osp_ic_clock_t clk;

	assert_golden_deserialize_roundtrip("data", osp_ic_data_class(), &data, sizeof(data), OSP_GV_data, OSP_GV_data_LEN);
	assert_true(osp_obis_eq(&data.logical_name, &OSP_FIXTURE_LN_DATA));
	assert_int_equal(osp_get_i8(&data.value), 42);

	assert_golden_deserialize_roundtrip("register", osp_ic_register_class(), &reg, sizeof(reg), OSP_GV_register,
	                                    OSP_GV_register_LEN);
	assert_true(osp_obis_eq(&reg.logical_name, &OSP_FIXTURE_LN_REGISTER_ENERGY));
	assert_int_equal(osp_get_i32(&reg.value), 1000);
	assert_int_equal(reg.scaler_unit.scaler, 0);
	assert_int_equal(reg.scaler_unit.unit, 27);

	assert_golden_deserialize_roundtrip("clock", osp_ic_clock_class(), &clk, sizeof(clk), OSP_GV_clock, OSP_GV_clock_LEN);
	assert_true(osp_obis_eq(&clk.logical_name, &OSP_FIXTURE_LN_CLOCK));
	assert_int_equal(clk.time.year, 2025);
	assert_int_equal(clk.time.month, 5);
	assert_int_equal(clk.time.day, 1);
	assert_int_equal(clk.time.hour, 16);
	assert_int_equal(clk.time.minute, 30);
	assert_int_equal(clk.timezone_offset, 180);
	assert_int_equal(clk.clock_status, 1);
	assert_int_equal(clk.clock_base, 2);
}

static void test_sto_cosem_datetime_octet(void **state) {
	(void)state;
	/* STO 34.01-5.1-013: date-time filter 2020-01-01, wildcards, deviation not used */
	static const uint8_t wire[] = {0x09, 0x0C, 0x07, 0xE4, 0x01, 0x01, 0xFF, 0x00,
	                               0x00, 0x00, 0x00, 0x80, 0x00, 0x00};
	osp_buf_t r;
	osp_buf_init(&r, (uint8_t *)wire, sizeof(wire));
	r.wr = sizeof(wire);
	osp_value_t v;
	assert_int_equal(osp_value_read(&r, &v), OSP_OK);
	osp_cosem_datetime_t dt;
	assert_int_equal(osp_cosem_datetime_read_value(&v, &dt), OSP_OK);
	assert_int_equal(dt.year, 2020);
	assert_int_equal(dt.month, 1);
	assert_int_equal(dt.day, 1);
	assert_int_equal(dt.day_of_week, 0xFF);
	assert_int_equal(dt.hour, 0);
	assert_int_equal(dt.minute, 0);
	assert_int_equal(dt.second, 0);
	assert_int_equal(dt.hundredths, 0);
	assert_int_equal(dt.deviation, (int16_t)0x8000);
	assert_int_equal(dt.clock_status, 0);

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_value_write(&w, &v), OSP_OK);
	assert_int_equal(w.wr, (uint32_t)sizeof(wire));
	assert_memory_equal(mem, wire, sizeof(wire));
}

static void test_profile_generic_spodes_buffer(void **state) {
	(void)state;
	osp_ic_profile_generic_t orig, copy;
	osp_fixture_profile_generic_spodes_buffer(&orig);

	uint8_t mem[512];
	osp_buf_t w, r;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_ic_serialize(osp_ic_profile_generic_class(), &orig, &w), OSP_OK);

	osp_ic_profile_generic_init(&copy, (osp_obis_t){0, 0, 0, 0, 0, 0});
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	assert_int_equal(osp_ic_deserialize(osp_ic_profile_generic_class(), &copy, &r), OSP_OK);

	assert_int_equal(copy.capture_period, 3600);
	assert_int_equal(copy.sort_method, 1);
	assert_int_equal(copy.entries_in_use, 1);
	assert_int_equal(copy.profile_entries, 100);
	assert_int_equal(copy.buffer.row_count, 1);
	assert_int_equal(copy.buffer.rows[0].cell_count, 2);
	assert_int_equal(osp_get_i32(&copy.buffer.rows[0].cells[0]), 1000);
	assert_int_equal(copy.buffer.rows[0].cells[1].tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(copy.buffer.rows[0].cells[1].as.octetstring.len, OSP_COSEM_DATETIME_LEN);
	assert_int_equal(copy.buffer.rows[0].cells[1].as.octetstring.data[0], 0x07);
	assert_int_equal(copy.buffer.rows[0].cells[1].as.octetstring.data[1], 0xE5);
}

static void test_all_ic_fixture_roundtrip(void **state) {
	(void)state;

	for (size_t i = 0; i < sizeof(fixture_table) / sizeof(fixture_table[0]); i++) {
		IC_FIXTURE_ROUNDTRIP(fixture_table[i].name, &fixture_table[i]);
	}
}

static void test_sap_assignment_connect(void **state) {
	(void)state;
	osp_ic_sap_assignment_t sap;
	osp_fixture_sap_assignment(&sap);

	static osp_value_t fields[2];
	fields[0] = osp_val_u16(1);
	fields[1].tag = OSP_TAG_OCTETSTRING;
	fields[1].as.octetstring.len = 3;
	memcpy(fields[1].as.octetstring.data, "LD1", 3);
	osp_value_t param = {.tag = OSP_TAG_STRUCTURE};
	param.as.structure.elements.items = fields;
	param.as.structure.elements.count = 2;

	osp_value_t result;
	assert_int_equal(osp_ic_sap_assignment_class()->invoke(&sap, 1, &param, &result), OSP_OK);
	assert_int_equal(sap.sap_list.count, 1);
	assert_int_equal(sap.sap_list.items[0].sap, 1);
}

static void test_mac_address_attrs(void **state) {
	(void)state;
	osp_ic_mac_address_t mac;
	osp_fixture_mac_address(&mac);

	osp_value_t v;
	static const uint8_t expected_mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
	assert_int_equal(osp_ic_mac_address_class()->get_attr(&mac, 2, &v), OSP_OK);
	assert_int_equal(v.as.octetstring.len, 6);
	assert_memory_equal(v.as.octetstring.data, expected_mac, 6);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_ic_spodes_golden_vectors),
	    cmocka_unit_test(test_ic_bluebook_register_table5),
	    cmocka_unit_test(test_ic_golden_field_values),
	    cmocka_unit_test(test_sto_cosem_datetime_octet),
	    cmocka_unit_test(test_profile_generic_spodes_buffer),
	    cmocka_unit_test(test_all_ic_fixture_roundtrip),
	    cmocka_unit_test(test_sap_assignment_connect),
	    cmocka_unit_test(test_mac_address_attrs),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
