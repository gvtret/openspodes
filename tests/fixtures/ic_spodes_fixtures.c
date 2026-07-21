/**
 * ic_spodes_fixtures.c — spodes-rs integration.rs aligned IC instances
 */

#include "ic_spodes_fixtures.h"
#include "codec/serialize.h"
#include <string.h>

/* integration.rs test_clock_serialization: 2025-05-01 Tue 16:30 */
static const uint8_t OSP_DT_2025_05_01_1630[] = {
    0x07, 0xE9, 0x05, 0x01, 0x02, 0x10, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* integration.rs daylight savings begin: 2025-03-26 02:00 */
static const uint8_t OSP_DT_DST_BEGIN[] = {
    0x07, 0xE9, 0x03, 0x26, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* integration.rs daylight savings end: 2025-10-29 02:00 */
static const uint8_t OSP_DT_DST_END[] = {
    0x07, 0xE9, 0x10, 0x29, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

osp_cosem_datetime_t osp_fixture_cosem_datetime_bytes(const uint8_t dt[12]) {
	osp_cosem_datetime_t out = {0};
	osp_cosem_datetime_from_bytes(&out, dt);
	return out;
}

osp_datetime_t osp_fixture_datetime_bytes(const uint8_t dt[12]) {
	osp_datetime_t out = {0};
	out.date.year = ((uint16_t)dt[0] << 8) | dt[1];
	out.date.month = dt[2];
	out.date.day = dt[3];
	out.date.day_of_week = dt[4];
	out.time.hour = dt[5];
	out.time.minute = dt[6];
	out.time.second = dt[7];
	out.time.ms = dt[8];
	return out;
}

void osp_fixture_capture_time_obis(const uint8_t dt[12], osp_obis_t *out) {
	memcpy(out, dt, 6);
}

void osp_fixture_data(void *inst) {
	osp_ic_data_t *d = (osp_ic_data_t *)inst;
	osp_ic_data_init(d, OSP_FIXTURE_LN_DATA);
	d->value = osp_val_i8(42);
}

void osp_fixture_register(void *inst) {
	/* spodes-rs integration.rs: DoubleLong(1000), OctetString [0, 27] (unit=W per Blue Book Table 4) */
	osp_ic_register_t *r = (osp_ic_register_t *)inst;
	osp_ic_register_init(r, OSP_FIXTURE_LN_REGISTER_ENERGY, osp_val_i32(1000));
	r->scaler_unit.scaler = 0;
	r->scaler_unit.unit = 27;
}

void osp_fixture_register_bluebook_kwh(void *inst) {
	/* Blue Book §4.3.2 Table 5 / IEC 62056-6-2 Table 6: 593 kWh */
	osp_ic_register_t *r = (osp_ic_register_t *)inst;
	osp_ic_register_init(r, OSP_FIXTURE_LN_ACTIVE_ENERGY_IMPORT, osp_val_u32(593));
	r->scaler_unit.scaler = 3;
	r->scaler_unit.unit = 30; /* Wh */
}

void osp_fixture_extended_register(void *inst) {
	osp_ic_ext_register_t *r = (osp_ic_ext_register_t *)inst;
	osp_ic_ext_register_init(r, OSP_FIXTURE_LN_EXT_REGISTER);
	r->value = osp_val_i32(2000);
	r->scaler_unit.scaler = 0;
	r->scaler_unit.unit = 27;
	r->status = osp_val_u8(1);
	osp_fixture_capture_time_obis(OSP_DT_2025_05_01_1630, &r->capture_time);
}

void osp_fixture_demand_register(void *inst) {
	osp_ic_demand_register_t *d = (osp_ic_demand_register_t *)inst;
	static const uint8_t start_time[] = {
	    0x07, 0xE9, 0x05, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	osp_ic_demand_register_init(d, OSP_FIXTURE_LN_DEMAND_REGISTER);
	d->current_average_value = osp_val_i32(3000);
	d->last_average_value = osp_val_i32(2500);
	d->scaler_unit.scaler = 0;
	d->scaler_unit.unit = 27;
	d->status = osp_val_u8(1);
	d->capture_time = osp_fixture_cosem_datetime_bytes(OSP_DT_2025_05_01_1630);
	d->start_time_current = osp_fixture_cosem_datetime_bytes(start_time);
	d->period = 3600;
	d->number_of_periods = 24;
}

void osp_fixture_clock(void *inst) {
	osp_ic_clock_t *c = (osp_ic_clock_t *)inst;
	osp_ic_clock_init(c, OSP_FIXTURE_LN_CLOCK);
	c->time = osp_fixture_cosem_datetime_bytes(OSP_DT_2025_05_01_1630);
	c->timezone_offset = 180;
	c->clock_status = 1;
	c->daylight_savings_begin = osp_fixture_cosem_datetime_bytes(OSP_DT_DST_BEGIN);
	c->daylight_savings_end = osp_fixture_cosem_datetime_bytes(OSP_DT_DST_END);
	c->daylight_savings_deviation = 60;
	c->daylight_savings_enabled = 1;
	c->clock_base = 2;
}

void osp_fixture_security_setup(void *inst) {
	osp_ic_security_setup_t *s = (osp_ic_security_setup_t *)inst;
	osp_ic_security_setup_init(s, OSP_FIXTURE_LN_SECURITY);
	s->security_policy = 3;
	s->security_suite = 1;
	s->client_system_title.len = 8;
	memcpy(s->client_system_title.data, "CLNT0001", 8);
	s->server_system_title.len = 8;
	memcpy(s->server_system_title.data, "SRVR0001", 8);
}

void osp_fixture_disconnect_control(void *inst) {
	osp_ic_disconnect_control_t *d = (osp_ic_disconnect_control_t *)inst;
	osp_ic_disconnect_control_init(d, OSP_FIXTURE_LN_DEFAULT);
	d->output_state = 1;
	d->control_state = 1;
	d->control_model = 4;
}

void osp_fixture_mbus_slave_port_setup(void *inst) {
	osp_ic_mbus_slave_port_setup_t *m = (osp_ic_mbus_slave_port_setup_t *)inst;
	osp_ic_mbus_slave_port_setup_init(m, (osp_obis_t){0, 0, 24, 1, 0, 255});
	m->default_baud = 2;
	m->bus_address = 42;
}

void osp_fixture_gprs_modem(void *inst) {
	osp_ic_gprs_modem_t *g = (osp_ic_gprs_modem_t *)inst;
	osp_ic_gprs_modem_init(g, (osp_obis_t){0, 0, 25, 4, 0, 255});
	memcpy(g->apn, "internet", 8);
	g->apn_len = 8;
	g->pin_code = 1234;
}

void osp_fixture_gsm_diagnostic(void *inst) {
	osp_ic_gsm_diagnostic_t *g = (osp_ic_gsm_diagnostic_t *)inst;
	osp_ic_gsm_diagnostic_init(g, (osp_obis_t){0, 0, 25, 6, 0, 255});
	memcpy(g->operator_name, "MegaFon", 7);
	g->operator_len = 7;
	g->status = 1;
	g->cs_attachment = 2;
	g->ps_status = 3;
	g->capture_time = osp_fixture_datetime_bytes(OSP_DT_2025_05_01_1630);
}

void osp_fixture_activity_calendar(void *inst) {
	osp_ic_activity_calendar_t *ac = (osp_ic_activity_calendar_t *)inst;
	osp_ic_activity_calendar_init(ac, (osp_obis_t){0, 0, 13, 0, 0, 255});
	memcpy(ac->calendar_name_active, "T1", 2);
	ac->calendar_name_active_len = 2;
	memcpy(ac->calendar_name_passive, "T2", 2);
	ac->calendar_name_passive_len = 2;
}

void osp_fixture_ipv4_setup(void *inst) {
	osp_ic_ipv4_setup_t *s = (osp_ic_ipv4_setup_t *)inst;
	osp_ic_ipv4_setup_init(s, (osp_obis_t){0, 0, 25, 1, 0, 255});
	s->dl_reference = (osp_obis_t){0, 0, 22, 0, 0, 255};
	s->ip_address = 0xC0A8010A; /* 192.168.1.10 */
	s->subnet_mask = 0xFFFFFF00;
	s->gateway_ip = 0xC0A80101;
	s->use_dhcp = false;
	s->primary_dns = 0x08080808;
	s->secondary_dns = 0x08080404;
}

void osp_fixture_ipv6_setup(void *inst) {
	osp_ic_ipv6_setup_t *s = (osp_ic_ipv6_setup_t *)inst;
	osp_ic_ipv6_setup_init(s, (osp_obis_t){0, 0, 25, 7, 0, 255});
	s->dl_reference = (osp_obis_t){0, 0, 22, 0, 0, 255};
	s->address_config_mode = 1;
	s->traffic_class = 0;
	static const uint8_t dns[] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
	memcpy(s->primary_dns, dns, sizeof(dns));
	s->primary_dns_len = sizeof(dns);
}

void osp_fixture_mac_address(void *inst) {
	osp_ic_mac_address_t *m = (osp_ic_mac_address_t *)inst;
	osp_ic_mac_address_init(m, (osp_obis_t){0, 0, 25, 1, 0, 255});
	m->mac_address_len = 6;
	memcpy(m->mac_address, (uint8_t[]){0x00, 0x11, 0x22, 0x33, 0x44, 0x55}, 6);
}

void osp_fixture_profile_generic(void *inst) {
	osp_ic_profile_generic_t *p = (osp_ic_profile_generic_t *)inst;
	osp_ic_profile_generic_init(p, OSP_FIXTURE_LN_PROFILE);
	p->capture_period = 3600;
	p->sort_method = (osp_sort_method_t)1;
	p->entries_in_use = 1;
	p->profile_entries = 100;
}

void osp_fixture_profile_generic_spodes_buffer(void *inst) {
	osp_fixture_profile_generic(inst);
	osp_ic_profile_generic_t *p = (osp_ic_profile_generic_t *)inst;
	static const uint8_t row_dt[] = {
	    0x07, 0xE5, 0x05, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	p->buffer.row_count = 1;
	p->buffer.rows[0].cell_count = 2;
	p->buffer.rows[0].cells[0] = osp_val_i32(1000);
	osp_cosem_datetime_t row_time = osp_fixture_cosem_datetime_bytes(row_dt);
	p->buffer.rows[0].cells[1] = osp_val_cosem_datetime(&row_time);
}

void osp_fixture_limiter(void *inst) {
	osp_ic_limiter_t *l = (osp_ic_limiter_t *)inst;
	osp_ic_limiter_init(l, (osp_obis_t){0, 0, 17, 0, 0, 255});
	l->monitored_value.class_id = 3;
	l->monitored_value.logical_name = (osp_obis_t){1, 0, 1, 7, 0, 255};
	l->monitored_value.attribute_index = 2;
	l->threshold_active = osp_val_f32(50.0f);
	l->threshold_normal = osp_val_null();
	l->threshold_emergency = osp_val_null();
	l->min_over_threshold_duration = 60;
	l->min_under_threshold_duration = 120;
}

void osp_fixture_register_monitor(void *inst) {
	osp_ic_register_monitor_t *rm = (osp_ic_register_monitor_t *)inst;
	osp_ic_register_monitor_init(rm, (osp_obis_t){0, 0, 21, 0, 0, 255});
}

void osp_fixture_sap_assignment(void *inst) {
	osp_ic_sap_assignment_t *s = (osp_ic_sap_assignment_t *)inst;
	osp_ic_sap_assignment_init(s, (osp_obis_t){0, 0, 41, 0, 0, 255});
}

void osp_fixture_single_action_schedule(void *inst) {
	osp_ic_single_action_schedule_t *s = (osp_ic_single_action_schedule_t *)inst;
	osp_ic_single_action_schedule_init(s, (osp_obis_t){0, 0, 15, 0, 0, 255});
	s->script_logical_name = (osp_obis_t){0, 0, 10, 0, 1, 255};
	s->script_selector = 0;
	s->schedule_type = 1;
	memset(s->execution_time[0].time, 0xFF, 4);
	memset(s->execution_time[0].date, 0xFF, 5);
	s->execution_time_count = 1;
}

void osp_fixture_iec_hdlc_setup(void *inst) {
	osp_ic_iec_hdlc_setup_t *h = (osp_ic_iec_hdlc_setup_t *)inst;
	osp_ic_iec_hdlc_setup_init(h, (osp_obis_t){0, 0, 22, 0, 0, 255});
	h->comm_speed = 5;
	h->window_size_transmit = 1;
	h->window_size_receive = 1;
	h->max_info_field_length_transmit = 128;
	h->max_info_field_length_receive = 128;
	h->inter_octet_time_out = 30;
	h->inactivity_time_out = 120;
	h->device_address = 16;
}

void osp_fixture_iec_local_port_setup(void *inst) {
	osp_ic_iec_local_port_setup_t *l = (osp_ic_iec_local_port_setup_t *)inst;
	osp_ic_iec_local_port_setup_init(l, (osp_obis_t){0, 0, 20, 0, 0, 255});
	l->default_mode = 1;
	l->default_baud_rate = 5;
}

void osp_fixture_tcp_udp_setup(void *inst) {
	osp_ic_tcp_udp_setup_t *t = (osp_ic_tcp_udp_setup_t *)inst;
	osp_ic_tcp_udp_setup_init(t, (osp_obis_t){0, 0, 25, 0, 0, 255});
	t->port = 4059;
	t->ip_setup_reference = (osp_obis_t){0, 0, 25, 1, 0, 255};
}

void osp_fixture_push_setup(void *inst) {
	osp_ic_push_setup_t *p = (osp_ic_push_setup_t *)inst;
	osp_ic_push_setup_init(p, (osp_obis_t){0, 0, 25, 9, 0, 255});
	p->push_object_list[0] = (osp_push_object_t){40, {0, 0, 25, 9, 0, 255}, 2, 0};
	p->push_object_count = 1;
	p->send_destination.transport_service = OSP_PUSH_TRANSPORT_TCP;
	const char *dest = "127.0.0.0:4059";
	memcpy(p->send_destination.destination, dest, strlen(dest));
	p->send_destination.destination_len = (uint32_t)strlen(dest);
}

void osp_fixture_image_transfer(void *inst) {
	osp_ic_image_transfer_t *i = (osp_ic_image_transfer_t *)inst;
	osp_ic_image_transfer_init(i, (osp_obis_t){0, 0, 44, 0, 0, 255});
}

void osp_fixture_arbitrator(void *inst) {
	osp_ic_arbitrator_t *a = (osp_ic_arbitrator_t *)inst;
	osp_ic_arbitrator_init(a, (osp_obis_t){0, 0, 1, 1, 0, 255});
}

void osp_fixture_data_protection(void *inst) {
	osp_ic_data_protection_t *d = (osp_ic_data_protection_t *)inst;
	osp_ic_data_protection_init(d, (osp_obis_t){0, 0, 30, 0, 0, 255});
}

void osp_fixture_association_ln(void *inst) {
	osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)inst;
	osp_ic_association_ln_init(a, (osp_obis_t){0, 0, 40, 0, 1, 255});
}

void osp_fixture_schedule(void *inst) {
	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	osp_ic_schedule_init(s, (osp_obis_t){0, 0, 10, 0, 1, 255});
}

void osp_fixture_script_table(void *inst) {
	osp_ic_script_table_t *s = (osp_ic_script_table_t *)inst;
	osp_ic_script_table_init(s, (osp_obis_t){0, 0, 10, 100, 0, 255});
}

void osp_fixture_special_days(void *inst) {
	osp_ic_special_days_t *s = (osp_ic_special_days_t *)inst;
	osp_ic_special_days_init(s, (osp_obis_t){0, 0, 11, 0, 0, 255});
}

void osp_fixture_register_activation(void *inst) {
	osp_ic_register_activation_t *r = (osp_ic_register_activation_t *)inst;
	osp_ic_register_activation_init(r, (osp_obis_t){0, 0, 10, 106, 0, 255});
}

void osp_fixture_compact_data(void *inst) {
	osp_ic_compact_data_t *c = (osp_ic_compact_data_t *)inst;
	osp_ic_compact_data_init(c, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_profile_filter(void *inst) {
	osp_ic_profile_filter_t *p = (osp_ic_profile_filter_t *)inst;
	osp_ic_profile_filter_init(p, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_profile_data_filter(void *inst) {
	osp_ic_profile_data_filter_t *p = (osp_ic_profile_data_filter_t *)inst;
	osp_ic_profile_data_filter_init(p, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_table_manager(void *inst) {
	osp_ic_table_manager_t *t = (osp_ic_table_manager_t *)inst;
	osp_ic_table_manager_init(t, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_register_table(void *inst) {
	osp_ic_register_table_t *r = (osp_ic_register_table_t *)inst;
	osp_ic_register_table_init(r, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_parameter_monitor(void *inst) {
	osp_ic_parameter_monitor_t *p = (osp_ic_parameter_monitor_t *)inst;
	osp_ic_parameter_monitor_init(p, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_mbus_slave(void *inst) {
	osp_ic_mbus_slave_t *m = (osp_ic_mbus_slave_t *)inst;
	osp_ic_mbus_slave_init(m, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_utility_tables(void *inst) {
	osp_ic_utility_tables_t *u = (osp_ic_utility_tables_t *)inst;
	osp_ic_utility_tables_init(u, OSP_FIXTURE_LN_DEFAULT);
}

void osp_fixture_status_mapping(void *inst) {
	osp_ic_status_mapping_t *s = (osp_ic_status_mapping_t *)inst;
	osp_ic_status_mapping_init(s, OSP_FIXTURE_LN_DEFAULT);
}
