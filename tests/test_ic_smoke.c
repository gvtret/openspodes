/**
 * test_ic_smoke.c — Smoke tests for all IC interface classes
 *
 * Exercises get/set/invoke vtables and dispatcher routing for every IC.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "server/dispatcher.h"

#include "ic/activity_calendar.h"
#include "ic/arbitrator.h"
#include "ic/association_ln.h"
#include "ic/clock.h"
#include "ic/compact_data.h"
#include "ic/data.h"
#include "ic/data_protection.h"
#include "ic/demand_register.h"
#include "ic/disconnect_control.h"
#include "ic/extended_register.h"
#include "ic/gprs_modem.h"
#include "ic/gsm_diagnostic.h"
#include "ic/iec_hdlc_setup.h"
#include "ic/iec_local_port_setup.h"
#include "ic/image_transfer.h"
#include "ic/ipv4_setup.h"
#include "ic/ipv6_setup.h"
#include "ic/limiter.h"
#include "ic/mac_address.h"
#include "ic/mbus_slave.h"
#include "ic/parameter_monitor.h"
#include "ic/profile_filter.h"
#include "ic/profile_generic.h"
#include "ic/push_setup.h"
#include "ic/register.h"
#include "ic/register_activation.h"
#include "ic/register_monitor.h"
#include "ic/register_table.h"
#include "ic/sap_assignment.h"
#include "ic/schedule.h"
#include "ic/script_table.h"
#include "ic/security_setup.h"
#include "ic/single_action_schedule.h"
#include "ic/special_days.h"
#include "ic/status_mapping.h"
#include "ic/table_manager.h"
#include "ic/tcp_udp_setup.h"
#include "ic/utility_tables.h"
#include "codec/structures.h"

static const osp_obis_t TEST_OBIS = {0, 0, 1, 0, 0, 255};

static void smoke_exercise(const osp_ic_class_t *cls, void *inst, const osp_obis_t *obis) {
	assert_non_null(cls);
	assert_non_null(cls->name);
	assert_true(cls->class_id > 0);
	assert_true(cls->instance_size > 0);

	int get_ok = 0;
	if (cls->get_attr) {
		for (uint8_t attr = 1; attr <= 12; attr++) {
			osp_value_t v;
			osp_err_t rc = cls->get_attr(inst, attr, &v);
			if (rc == OSP_OK) {
				get_ok++;
			} else {
				assert_true(rc == OSP_ERR_NOT_FOUND || rc == OSP_ERR_INVALID || rc == OSP_ERR_UNSUPPORTED);
			}
		}
	}

	if (cls->set_attr) {
		osp_value_t v = osp_val_u32(1);
		for (uint8_t attr = 2; attr <= 10; attr++) {
			(void)cls->set_attr(inst, attr, &v);
		}
	}

	if (cls->invoke) {
		for (uint8_t method = 1; method <= 6; method++) {
			osp_value_t result;
			(void)cls->invoke(inst, method, NULL, &result);
		}
	}

	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);
	assert_int_equal(osp_dispatcher_register(&disp, cls, inst), OSP_OK);

	osp_value_t routed;
	if (get_ok > 0) {
		for (uint8_t attr = 1; attr <= 12; attr++) {
			if (osp_dispatcher_get(&disp, cls->class_id, obis, attr, &routed) == OSP_OK) {
				break;
			}
		}
	}

	osp_value_t setv = osp_val_u32(7);
	(void)osp_dispatcher_set(&disp, cls->class_id, obis, 2, &setv);
	(void)osp_dispatcher_action(&disp, cls->class_id, obis, 1, NULL, &routed);
}

#define SMOKE_IC(Type, init_stmt, class_fn)            \
	do {                                         \
		Type _inst;                          \
		init_stmt;                           \
		smoke_exercise(class_fn(), &_inst, &TEST_OBIS); \
	} while (0)

static void test_ic_all_smoke(void **state) {
	(void)state;

	SMOKE_IC(osp_ic_data_t, osp_ic_data_init(&_inst, TEST_OBIS), osp_ic_data_class);
	SMOKE_IC(osp_ic_register_t, osp_ic_register_init(&_inst, TEST_OBIS, osp_val_u32(10)), osp_ic_register_class);
	SMOKE_IC(osp_ic_ext_register_t, osp_ic_ext_register_init(&_inst, TEST_OBIS), osp_ic_ext_register_class);
	SMOKE_IC(osp_ic_clock_t, osp_ic_clock_init(&_inst, TEST_OBIS), osp_ic_clock_class);
	SMOKE_IC(osp_ic_profile_generic_t, osp_ic_profile_generic_init(&_inst, TEST_OBIS), osp_ic_profile_generic_class);
	SMOKE_IC(osp_ic_disconnect_control_t, osp_ic_disconnect_control_init(&_inst, TEST_OBIS), osp_ic_disconnect_control_class);
	SMOKE_IC(osp_ic_security_setup_t, osp_ic_security_setup_init(&_inst, TEST_OBIS), osp_ic_security_setup_class);
	SMOKE_IC(osp_ic_limiter_t, osp_ic_limiter_init(&_inst, TEST_OBIS), osp_ic_limiter_class);
	SMOKE_IC(osp_ic_schedule_t, osp_ic_schedule_init(&_inst, TEST_OBIS), osp_ic_schedule_class);
	SMOKE_IC(osp_ic_script_table_t, osp_ic_script_table_init(&_inst, TEST_OBIS), osp_ic_script_table_class);
	SMOKE_IC(osp_ic_special_days_t, osp_ic_special_days_init(&_inst, TEST_OBIS), osp_ic_special_days_class);
	SMOKE_IC(osp_ic_register_activation_t, osp_ic_register_activation_init(&_inst, TEST_OBIS), osp_ic_register_activation_class);
	SMOKE_IC(osp_ic_sap_assignment_t, osp_ic_sap_assignment_init(&_inst, TEST_OBIS), osp_ic_sap_assignment_class);
	SMOKE_IC(osp_ic_image_transfer_t, osp_ic_image_transfer_init(&_inst, TEST_OBIS), osp_ic_image_transfer_class);
	SMOKE_IC(osp_ic_activity_calendar_t, osp_ic_activity_calendar_init(&_inst, TEST_OBIS), osp_ic_activity_calendar_class);
	SMOKE_IC(osp_ic_register_monitor_t, osp_ic_register_monitor_init(&_inst, TEST_OBIS), osp_ic_register_monitor_class);
	SMOKE_IC(osp_ic_single_action_schedule_t, osp_ic_single_action_schedule_init(&_inst, TEST_OBIS), osp_ic_single_action_schedule_class);
	SMOKE_IC(osp_ic_ipv4_setup_t, osp_ic_ipv4_setup_init(&_inst, TEST_OBIS), osp_ic_ipv4_setup_class);
	SMOKE_IC(osp_ic_tcp_udp_setup_t, osp_ic_tcp_udp_setup_init(&_inst, TEST_OBIS), osp_ic_tcp_udp_setup_class);
	SMOKE_IC(osp_ic_profile_filter_t, osp_ic_profile_filter_init(&_inst, TEST_OBIS), osp_ic_profile_filter_class);
	SMOKE_IC(osp_ic_arbitrator_t, osp_ic_arbitrator_init(&_inst, TEST_OBIS), osp_ic_arbitrator_class);
	SMOKE_IC(osp_ic_demand_register_t, osp_ic_demand_register_init(&_inst, TEST_OBIS), osp_ic_demand_register_class);
	SMOKE_IC(osp_ic_status_mapping_t, osp_ic_status_mapping_init(&_inst, TEST_OBIS), osp_ic_status_mapping_class);
	SMOKE_IC(osp_ic_utility_tables_t, osp_ic_utility_tables_init(&_inst, TEST_OBIS), osp_ic_utility_tables_class);
	SMOKE_IC(osp_ic_compact_data_t, osp_ic_compact_data_init(&_inst, TEST_OBIS), osp_ic_compact_data_class);
	SMOKE_IC(osp_ic_iec_hdlc_setup_t, osp_ic_iec_hdlc_setup_init(&_inst, TEST_OBIS), osp_ic_iec_hdlc_setup_class);
	SMOKE_IC(osp_ic_iec_local_port_setup_t, osp_ic_iec_local_port_setup_init(&_inst, TEST_OBIS), osp_ic_iec_local_port_setup_class);
	SMOKE_IC(osp_ic_data_protection_t, osp_ic_data_protection_init(&_inst, TEST_OBIS), osp_ic_data_protection_class);
	SMOKE_IC(osp_ic_push_setup_t, osp_ic_push_setup_init(&_inst, TEST_OBIS), osp_ic_push_setup_class);
	SMOKE_IC(osp_ic_mac_address_t, osp_ic_mac_address_init(&_inst, TEST_OBIS), osp_ic_mac_address_class);
	SMOKE_IC(osp_ic_gprs_modem_t, osp_ic_gprs_modem_init(&_inst, TEST_OBIS), osp_ic_gprs_modem_class);
	SMOKE_IC(osp_ic_gsm_diagnostic_t, osp_ic_gsm_diagnostic_init(&_inst, TEST_OBIS), osp_ic_gsm_diagnostic_class);
	SMOKE_IC(osp_ic_ipv6_setup_t, osp_ic_ipv6_setup_init(&_inst, TEST_OBIS), osp_ic_ipv6_setup_class);
	SMOKE_IC(osp_ic_register_table_t, osp_ic_register_table_init(&_inst, TEST_OBIS), osp_ic_register_table_class);
	SMOKE_IC(osp_ic_parameter_monitor_t, osp_ic_parameter_monitor_init(&_inst, TEST_OBIS), osp_ic_parameter_monitor_class);
	SMOKE_IC(osp_ic_mbus_slave_t, osp_ic_mbus_slave_init(&_inst, TEST_OBIS), osp_ic_mbus_slave_class);
	SMOKE_IC(osp_ic_table_manager_t, osp_ic_table_manager_init(&_inst, TEST_OBIS), osp_ic_table_manager_class);
}

static void test_ic_association_ln_smoke(void **state) {
	(void)state;

	osp_ic_association_ln_t aln;
	osp_ic_association_ln_init(&aln, (osp_obis_t){0, 0, 40, 0, 0, 255});

	osp_object_list_element_t elem = {
	    .class_id = 3,
	    .logical_name = TEST_OBIS,
	    .version = 0,
	};
	elem.access_rights.attr_count = 1;
	elem.access_rights.attr_items[0].attribute_id = 2;
	elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_READ_WRITE;
	elem.access_rights.method_count = 1;
	elem.access_rights.method_items[0].method_id = 1;
	elem.access_rights.method_items[0].access_mode = OSP_METHOD_ACCESS;

	assert_int_equal(osp_ic_association_ln_add_object(&aln, &elem), OSP_OK);
	assert_non_null(osp_ic_association_ln_find_object(&aln, 3, &TEST_OBIS));
	assert_true(osp_ic_association_ln_can_read(&aln, 3, &TEST_OBIS, 2));
	assert_true(osp_ic_association_ln_can_write(&aln, 3, &TEST_OBIS, 2));
	assert_true(osp_ic_association_ln_can_invoke(&aln, 3, &TEST_OBIS, 1));
	assert_false(osp_ic_association_ln_can_read(&aln, 3, &TEST_OBIS, 99));

	osp_object_list_element_t auth_elem = elem;
	auth_elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_AUTH_READ_ONLY;
	assert_int_equal(osp_ic_association_ln_remove_object(&aln, 3, &TEST_OBIS), OSP_OK);
	assert_int_equal(osp_ic_association_ln_add_object(&aln, &auth_elem), OSP_OK);
	assert_true(osp_ic_association_ln_can_read(&aln, 3, &TEST_OBIS, 2));
	assert_false(osp_ic_association_ln_can_write(&aln, 3, &TEST_OBIS, 2));

	auth_elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_AUTH_WRITE_ONLY;
	assert_int_equal(osp_ic_association_ln_remove_object(&aln, 3, &TEST_OBIS), OSP_OK);
	assert_int_equal(osp_ic_association_ln_add_object(&aln, &auth_elem), OSP_OK);
	assert_false(osp_ic_association_ln_can_read(&aln, 3, &TEST_OBIS, 2));
	assert_true(osp_ic_association_ln_can_write(&aln, 3, &TEST_OBIS, 2));

	auth_elem.access_rights.attr_items[0].access_mode = OSP_ACCESS_AUTH_READ_WRITE;
	assert_int_equal(osp_ic_association_ln_remove_object(&aln, 3, &TEST_OBIS), OSP_OK);
	assert_int_equal(osp_ic_association_ln_add_object(&aln, &auth_elem), OSP_OK);
	assert_true(osp_ic_association_ln_can_read(&aln, 3, &TEST_OBIS, 2));
	assert_true(osp_ic_association_ln_can_write(&aln, 3, &TEST_OBIS, 2));

	const osp_ic_class_t *cls = osp_ic_association_ln_class();
	osp_value_t result;
	assert_int_equal(cls->invoke(&aln, 3, NULL, &result), OSP_OK);
	assert_int_equal(cls->invoke(&aln, 1, NULL, &result), OSP_ERR_NOT_FOUND);

	assert_int_equal(osp_ic_association_ln_remove_object(&aln, 3, &TEST_OBIS), OSP_OK);
	assert_null(osp_ic_association_ln_find_object(&aln, 3, &TEST_OBIS));

	smoke_exercise(cls, &aln, &aln.logical_name);
}

static void test_ic_dispatcher_not_found(void **state) {
	(void)state;

	osp_dispatcher_t disp;
	osp_dispatcher_init(&disp);

	osp_value_t v;
	osp_value_t setv = osp_val_u32(1);
	assert_int_equal(osp_dispatcher_get(&disp, 99, &TEST_OBIS, 2, &v), OSP_ERR_NOT_FOUND);
	assert_int_equal(osp_dispatcher_set(&disp, 99, &TEST_OBIS, 2, &setv), OSP_ERR_NOT_FOUND);
	assert_int_equal(osp_dispatcher_action(&disp, 99, &TEST_OBIS, 1, NULL, &v), OSP_ERR_NOT_FOUND);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_ic_all_smoke),
	    cmocka_unit_test(test_ic_association_ln_smoke),
	    cmocka_unit_test(test_ic_dispatcher_not_found),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
