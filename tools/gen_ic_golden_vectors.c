/**
 * gen_ic_golden_vectors.c — emit tests/fixtures/ic_golden_vectors.h
 *
 * Run from build dir after configuring:
 *   cmake --build build-linux --target gen_ic_golden_vectors
 *   ./build-linux/gen_ic_golden_vectors > tests/fixtures/ic_golden_vectors.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codec/ic_serialize.h"
#include "fixtures/ic_spodes_fixtures.h"

typedef void (*osp_fixture_init_fn)(void *);

typedef struct {
	const char *name;
	const osp_ic_class_t *(*class_fn)(void);
	size_t inst_size;
	osp_fixture_init_fn init;
} osp_ic_vector_spec_t;

#define SPEC(label, Type, class_fn, init_fn) \
	{label, class_fn, sizeof(Type), init_fn}

static const osp_ic_vector_spec_t specs[] = {
    SPEC("data", osp_ic_data_t, osp_ic_data_class, osp_fixture_data),
    SPEC("register", osp_ic_register_t, osp_ic_register_class, osp_fixture_register),
    SPEC("extended_register", osp_ic_ext_register_t, osp_ic_ext_register_class, osp_fixture_extended_register),
    SPEC("demand_register", osp_ic_demand_register_t, osp_ic_demand_register_class, osp_fixture_demand_register),
    SPEC("clock", osp_ic_clock_t, osp_ic_clock_class, osp_fixture_clock),
    SPEC("profile_generic", osp_ic_profile_generic_t, osp_ic_profile_generic_class, osp_fixture_profile_generic),
    SPEC("disconnect_control", osp_ic_disconnect_control_t, osp_ic_disconnect_control_class, osp_fixture_disconnect_control),
    SPEC("security_setup", osp_ic_security_setup_t, osp_ic_security_setup_class, osp_fixture_security_setup),
    SPEC("limiter", osp_ic_limiter_t, osp_ic_limiter_class, osp_fixture_limiter),
    SPEC("schedule", osp_ic_schedule_t, osp_ic_schedule_class, osp_fixture_schedule),
    SPEC("script_table", osp_ic_script_table_t, osp_ic_script_table_class, osp_fixture_script_table),
    SPEC("special_days", osp_ic_special_days_t, osp_ic_special_days_class, osp_fixture_special_days),
    SPEC("register_activation", osp_ic_register_activation_t, osp_ic_register_activation_class, osp_fixture_register_activation),
    SPEC("sap_assignment", osp_ic_sap_assignment_t, osp_ic_sap_assignment_class, osp_fixture_sap_assignment),
    SPEC("image_transfer", osp_ic_image_transfer_t, osp_ic_image_transfer_class, osp_fixture_image_transfer),
    SPEC("activity_calendar", osp_ic_activity_calendar_t, osp_ic_activity_calendar_class, osp_fixture_activity_calendar),
    SPEC("register_monitor", osp_ic_register_monitor_t, osp_ic_register_monitor_class, osp_fixture_register_monitor),
    SPEC("single_action_schedule", osp_ic_single_action_schedule_t, osp_ic_single_action_schedule_class, osp_fixture_single_action_schedule),
    SPEC("ipv4_setup", osp_ic_ipv4_setup_t, osp_ic_ipv4_setup_class, osp_fixture_ipv4_setup),
    SPEC("tcp_udp_setup", osp_ic_tcp_udp_setup_t, osp_ic_tcp_udp_setup_class, osp_fixture_tcp_udp_setup),
    SPEC("arbitrator", osp_ic_arbitrator_t, osp_ic_arbitrator_class, osp_fixture_arbitrator),
    SPEC("iec_hdlc_setup", osp_ic_iec_hdlc_setup_t, osp_ic_iec_hdlc_setup_class, osp_fixture_iec_hdlc_setup),
    SPEC("iec_local_port_setup", osp_ic_iec_local_port_setup_t, osp_ic_iec_local_port_setup_class, osp_fixture_iec_local_port_setup),
    SPEC("data_protection", osp_ic_data_protection_t, osp_ic_data_protection_class, osp_fixture_data_protection),
    SPEC("push_setup", osp_ic_push_setup_t, osp_ic_push_setup_class, osp_fixture_push_setup),
    SPEC("mac_address", osp_ic_mac_address_t, osp_ic_mac_address_class, osp_fixture_mac_address),
    SPEC("gprs_modem", osp_ic_gprs_modem_t, osp_ic_gprs_modem_class, osp_fixture_gprs_modem),
    SPEC("gsm_diagnostic", osp_ic_gsm_diagnostic_t, osp_ic_gsm_diagnostic_class, osp_fixture_gsm_diagnostic),
    SPEC("ipv6_setup", osp_ic_ipv6_setup_t, osp_ic_ipv6_setup_class, osp_fixture_ipv6_setup),
    SPEC("mbus_slave_port_setup", osp_ic_mbus_slave_port_setup_t, osp_ic_mbus_slave_port_setup_class, osp_fixture_mbus_slave_port_setup),
    SPEC("profile_data_filter", osp_ic_profile_data_filter_t, osp_ic_profile_data_filter_class, osp_fixture_profile_data_filter),
    SPEC("table_manager", osp_ic_table_manager_t, osp_ic_table_manager_class, osp_fixture_table_manager),
    SPEC("register_table", osp_ic_register_table_t, osp_ic_register_table_class, osp_fixture_register_table),
    SPEC("parameter_monitor", osp_ic_parameter_monitor_t, osp_ic_parameter_monitor_class, osp_fixture_parameter_monitor),
    SPEC("mbus_slave", osp_ic_mbus_slave_t, osp_ic_mbus_slave_class, osp_fixture_mbus_slave),
    SPEC("utility_tables", osp_ic_utility_tables_t, osp_ic_utility_tables_class, osp_fixture_utility_tables),
    SPEC("status_mapping", osp_ic_status_mapping_t, osp_ic_status_mapping_class, osp_fixture_status_mapping),
    SPEC("compact_data", osp_ic_compact_data_t, osp_ic_compact_data_class, osp_fixture_compact_data),
    SPEC("profile_filter", osp_ic_profile_filter_t, osp_ic_profile_filter_class, osp_fixture_profile_filter),
    SPEC("association_ln", osp_ic_association_ln_t, osp_ic_association_ln_class, osp_fixture_association_ln),
};

static void print_bytes(const char *sym, const uint8_t *buf, uint32_t len) {
	printf("static const uint8_t %s[] = {", sym);
	for (uint32_t i = 0; i < len; i++) {
		if (i % 12 == 0) {
			printf("\n    ");
		}
		printf("0x%02X%s", buf[i], (i + 1 < len) ? ", " : "");
	}
	printf("\n};\n");
}

int main(void) {
	printf("/* Auto-generated by tools/gen_ic_golden_vectors.c — do not edit by hand. */\n");
	printf("/* Fixtures: tests/fixtures/ic_spodes_fixtures.c (spodes-rs integration.rs). */\n");
	printf("#ifndef OSP_IC_GOLDEN_VECTORS_H\n");
	printf("#define OSP_IC_GOLDEN_VECTORS_H\n\n");
	printf("#include <stddef.h>\n");
	printf("#include <stdint.h>\n\n");

	for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
		const osp_ic_vector_spec_t *s = &specs[i];
		const osp_ic_class_t *cls = s->class_fn();
		void *inst = calloc(1, s->inst_size);
		uint8_t mem[8192];
		osp_buf_t w;

		if (!inst) {
			fprintf(stderr, "calloc failed: %s\n", s->name);
			return 1;
		}
		s->init(inst);
		osp_buf_init(&w, mem, sizeof(mem));
		if (osp_ic_serialize(cls, inst, &w) != OSP_OK) {
			fprintf(stderr, "serialize failed: %s\n", s->name);
			free(inst);
			return 1;
		}

		char sym[128];
		snprintf(sym, sizeof(sym), "osp_gv_%s", s->name);
		for (char *p = sym; *p; p++) {
			if (*p == '-') {
				*p = '_';
			}
		}
		printf("/* %s class_id=%u len=%u */\n", s->name, cls->class_id, w.wr);
		print_bytes(sym, mem, w.wr);
		printf("#define OSP_GV_%s %s\n", s->name, sym);
		printf("#define OSP_GV_%s_LEN %uu\n\n", s->name, w.wr);
		free(inst);
	}

	printf("typedef struct {\n");
	printf("\tconst char *name;\n");
	printf("\tuint16_t class_id;\n");
	printf("\tconst uint8_t *bytes;\n");
	printf("\tsize_t len;\n");
	printf("} osp_ic_golden_vector_t;\n\n");

	printf("static const osp_ic_golden_vector_t osp_ic_golden_vectors[] = {\n");
	for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
		const osp_ic_class_t *cls = specs[i].class_fn();
		printf("\t{\"%s\", %u, OSP_GV_%s, OSP_GV_%s_LEN},\n", specs[i].name, cls->class_id, specs[i].name,
		       specs[i].name);
	}
	printf("};\n\n");
	printf("static const size_t osp_ic_golden_vector_count = sizeof(osp_ic_golden_vectors) / sizeof(osp_ic_golden_vectors[0]);\n\n");
	printf("#endif /* OSP_IC_GOLDEN_VECTORS_H */\n");
	return 0;
}
