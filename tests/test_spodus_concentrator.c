/**
 * test_spodus_concentrator.c — SPODUS concentrator runtime (registry, proxy, poll)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "spodus/concentrator.h"
#include "spodus/spodus_data.h"
#include "spodus/spodus_obis.h"
#include "server/server.h"
#include "client/client.h"
#include "ic/data.h"
#include "service/service.h"
#include "mock_transport.h"
#include "mock_crypto.h"

typedef struct {
	mock_transport_pair_t *pair;
	osp_server_t *meter;
} downstream_ctx_t;

static osp_server_t *g_ivke_server;

static osp_err_t ivke_loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_ivke_server, data, len);
}

static osp_err_t ivke_loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout_ms);
}

static osp_err_t downstream_send(void *ctx, const uint8_t *data, uint32_t len) {
	downstream_ctx_t *dc = (downstream_ctx_t *)ctx;
	return mock_loopback_send(dc->pair, dc->meter, data, len);
}

static osp_err_t downstream_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	downstream_ctx_t *dc = (downstream_ctx_t *)ctx;
	return mock_recv_from_peer(&dc->pair->client_rx, buf, size, out_len, timeout_ms);
}

static osp_ic_data_t g_meter_data;

static void setup_meter_server(osp_server_t *meter, mock_transport_pair_t *pair, downstream_ctx_t *dc, osp_obis_t obis, uint32_t energy) {
	mock_transport_pair_init(pair);
	osp_server_init(meter, &pair->server_transport, OSP_FRAMING_NONE);
	osp_ic_data_init(&g_meter_data, obis);
	g_meter_data.value = osp_val_u32(energy);
	osp_server_register(meter, osp_ic_data_class(), &g_meter_data);

	dc->pair = pair;
	dc->meter = meter;
	pair->client_transport.ctx = dc;
	pair->client_transport.send = downstream_send;
	pair->client_transport.recv = downstream_recv;
}

static void test_registry_meter_list_and_cache(void **state) {
	(void)state;
	osp_spodus_meter_registry_t reg;
	osp_spodus_registry_init(&reg);

	static const uint8_t mid[] = "SIT12260004";
	osp_spodus_meter_descriptor_t desc = {0};
	desc.meter_id_len = (uint8_t)strlen((const char *)mid);
	memcpy(desc.meter_id, mid, desc.meter_id_len);
	desc.meter_model_len = 3;
	memcpy(desc.meter_model, "ABC", 3);
	desc.channel_count = 1;
	desc.channels[0].id = 1;
	desc.channels[0].address_len = 1;
	desc.channels[0].address[0] = 0x11;

	assert_int_equal(osp_spodus_registry_add(&reg, &desc), OSP_OK);
	assert_non_null(osp_spodus_registry_find(&reg, mid, desc.meter_id_len));

	osp_obis_t energy = {1, 0, 1, 8, 0, 255};
	osp_value_t val = osp_val_u32(123456);
	assert_int_equal(osp_spodus_registry_store(&reg, mid, desc.meter_id_len, energy, 2, &val), OSP_OK);
	const osp_value_t *cached = osp_spodus_registry_cached(&reg, mid, desc.meter_id_len, &energy, 2);
	assert_non_null(cached);
	assert_int_equal(cached->as.uint32.value, 123456);

	osp_value_t list;
	assert_int_equal(osp_spodus_registry_build_meter_list(&reg, &list), OSP_OK);
	assert_int_equal(list.tag, OSP_TAG_ARRAY);
	assert_int_equal(list.as.array.elements.count, 1);

	osp_spodus_registry_remove(&reg, mid, desc.meter_id_len);
	assert_null(osp_spodus_registry_find(&reg, mid, desc.meter_id_len));
	assert_null(osp_spodus_registry_cached(&reg, mid, desc.meter_id_len, &energy, 2));
}

static void test_direct_channel_table(void **state) {
	(void)state;
	osp_spodus_direct_channel_table_t table;
	osp_spodus_direct_table_init(&table);

	static const uint8_t mid[] = "MTR001";
	osp_spodus_direct_channel_t row = {.direct_id = 200, .meter_id_len = 6, .channel_id = 1};
	memcpy(row.meter_id, mid, 6);
	assert_int_equal(osp_spodus_direct_table_add(&table, &row), OSP_OK);
	assert_non_null(osp_spodus_direct_table_find(&table, 200));

	osp_value_t val;
	assert_int_equal(osp_spodus_direct_table_build_value(&table, &val), OSP_OK);
	assert_int_equal(val.as.array.elements.count, 1);
	assert_int_equal(val.as.array.elements.items[0].as.structure.elements.items[0].as.uint16.value, 200);
}

static void test_channel_list_builds_profile(void **state) {
	(void)state;
	osp_spodus_channel_list_t list;
	osp_spodus_channel_list_init(&list);
	osp_spodus_channel_t channel = {.channel_id = 1, .interface_len = 12};
	memcpy(channel.interface, "RS485_1:9600", channel.interface_len);
	assert_int_equal(osp_spodus_channel_list_add(&list, &channel), OSP_OK);

	osp_ic_profile_generic_t profile;
	assert_int_equal(osp_spodus_channel_list_build_profile(&list, &profile), OSP_OK);
	osp_obis_t channels_obis = osp_spodus_obis_channel_list();
	assert_true(osp_obis_eq(&profile.logical_name, &channels_obis));
	assert_int_equal(profile.buffer.row_count, 1);
	assert_int_equal(profile.buffer.rows[0].cells[0].as.uint8.value, 1);
	assert_int_equal(profile.buffer.rows[0].cells[1].as.octetstring.len, 12);
	assert_memory_equal(profile.buffer.rows[0].cells[1].as.octetstring.data, "RS485_1:9600", 12);
}

static void test_discovered_meters_build_profile(void **state) {
	(void)state;
	osp_spodus_discovered_list_t list;
	osp_spodus_discovered_list_init(&list);
	osp_spodus_discovered_meter_t meter = {.meter_id_len = 8, .meter_model_len = 3, .channel_id = 1, .address = 0x10, .first_seen_len = 4,
	                                       .last_seen_len = 4};
	memcpy(meter.meter_id, "MTR-0001", 8);
	memcpy(meter.meter_model, "SiT", 3);
	memcpy(meter.first_seen, "\x07\xE6\x07\x04", 4);
	memcpy(meter.last_seen, "\x07\xE6\x07\x05", 4);
	assert_int_equal(osp_spodus_discovered_list_add(&list, &meter), OSP_OK);

	osp_ic_profile_generic_t profile;
	assert_int_equal(osp_spodus_discovered_list_build_profile(&list, &profile), OSP_OK);
	assert_int_equal(profile.buffer.row_count, 1);
	assert_int_equal(profile.buffer.rows[0].cell_count, 6);
	assert_memory_equal(profile.buffer.rows[0].cells[0].as.octetstring.data, "MTR-0001", 8);
	assert_int_equal(profile.buffer.rows[0].cells[2].as.uint8.value, 1);
	assert_int_equal(profile.buffer.rows[0].cells[3].as.uint16.value, 0x10);
}

static void test_access_policies_build_value(void **state) {
	(void)state;
	osp_spodus_access_policies_t policies;
	osp_spodus_access_policies_init(&policies);
	osp_spodus_access_policy_t policy = {.meter_id_len = 11, .policy_id = 3, .suite_id = 0, .security_count = 1};
	memcpy(policy.meter_id, "SIT12260004", policy.meter_id_len);
	policy.security[0].type = OSP_SPODUS_SEC_LLS_PASSWORD;
	policy.security[0].key_len = 8;
	memcpy(policy.security[0].key, "12345678", 8);
	assert_int_equal(osp_spodus_access_policies_add(&policies, &policy), OSP_OK);

	osp_value_t value;
	assert_int_equal(osp_spodus_access_policies_build_value(&policies, &value), OSP_OK);
	assert_int_equal(value.tag, OSP_TAG_ARRAY);
	assert_int_equal(value.as.array.elements.count, 1);
	osp_value_t *row = &value.as.array.elements.items[0];
	assert_int_equal(row->as.structure.elements.count, 4);
	assert_int_equal(row->as.structure.elements.items[1].as.uint8.value, 3);
	assert_int_equal(row->as.structure.elements.items[3].as.array.elements.count, 1);
	assert_memory_equal(row->as.structure.elements.items[3].as.array.elements.items[0].as.structure.elements.items[1].as.octetstring.data,
	                    "12345678", 8);
}

static void test_poll_meter_updates_cache(void **state) {
	(void)state;
	mock_crypto_init();

	mock_transport_pair_t meter_pair;
	downstream_ctx_t meter_ctx;
	osp_server_t meter_server;
	osp_obis_t energy = {1, 0, 1, 8, 0, 255};
	setup_meter_server(&meter_server, &meter_pair, &meter_ctx, energy, 123456);

	osp_spodus_concentrator_t conc;
	osp_spodus_concentrator_init(&conc);

	static const uint8_t mid[] = "SIT12260004";
	osp_spodus_meter_descriptor_t desc = {0};
	desc.meter_id_len = (uint8_t)strlen((const char *)mid);
	memcpy(desc.meter_id, mid, desc.meter_id_len);
	assert_int_equal(osp_spodus_registry_add(&conc.registry, &desc), OSP_OK);

	assert_int_equal(osp_spodus_concentrator_attach_downstream(&conc, mid, desc.meter_id_len, &meter_pair.client_transport, OSP_FRAMING_NONE),
	                 OSP_OK);
	assert_int_equal(osp_spodus_concentrator_connect_downstream(&conc, mid, desc.meter_id_len, 5000), OSP_OK);

	osp_spodus_downstream_t *link = osp_spodus_concentrator_downstream(&conc, mid, desc.meter_id_len);
	assert_non_null(link);

	osp_spodus_attr_ref_t attrs[] = {{1, energy, 2}};
	uint32_t n = osp_spodus_poll_meter(&link->client, &conc.registry, mid, desc.meter_id_len, attrs, 1);
	assert_int_equal(n, 1);
	const osp_value_t *cached = osp_spodus_registry_cached(&conc.registry, mid, desc.meter_id_len, &energy, 2);
	assert_non_null(cached);
	assert_int_equal(cached->as.uint32.value, 123456);
}

static void test_proxy_forward_roundtrip(void **state) {
	(void)state;
	mock_crypto_init();

	mock_transport_pair_t meter_pair;
	downstream_ctx_t meter_ctx;
	osp_server_t meter_server;
	osp_obis_t obis = {0, 0, 0x90, 0, 0, 255};
	setup_meter_server(&meter_server, &meter_pair, &meter_ctx, obis, 4242);

	osp_spodus_concentrator_t conc;
	osp_spodus_concentrator_init(&conc);

	static const uint8_t mid[] = "MTR001";
	osp_spodus_direct_channel_t row = {.direct_id = 200, .meter_id_len = 6, .channel_id = 1};
	memcpy(row.meter_id, mid, 6);
	assert_int_equal(osp_spodus_direct_table_add(&conc.direct, &row), OSP_OK);
	assert_int_equal(osp_spodus_concentrator_attach_downstream(&conc, mid, 6, &meter_pair.client_transport, OSP_FRAMING_NONE), OSP_OK);
	assert_int_equal(osp_spodus_concentrator_connect_downstream(&conc, mid, 6, 5000), OSP_OK);

	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = 0x01;
	req.as.normal.attr.class_id = 1;
	req.as.normal.attr.instance_id = obis;
	req.as.normal.attr.attribute_id = 1;

	uint8_t apdu[64];
	osp_buf_t buf;
	osp_buf_init(&buf, apdu, sizeof(apdu));
	assert_int_equal(osp_get_request_encode(&buf, &req), 0);

	uint8_t resp[256];
	uint32_t resp_len = 0;
	assert_int_equal(osp_spodus_proxy_forward(&conc, 200, apdu, buf.wr, resp, sizeof(resp), &resp_len, 5000), OSP_OK);
	assert_true(resp_len > 0);
	assert_int_equal(resp[0], OSP_TAG_GET_RESPONSE);
}

static void test_concentrator_server_get_objects(void **state) {
	(void)state;
	mock_crypto_init();

	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	osp_spodus_concentrator_t conc;
	osp_spodus_concentrator_init(&conc);

	static const uint8_t mid[] = "SIT12260004";
	osp_spodus_meter_descriptor_t desc = {0};
	desc.meter_id_len = (uint8_t)strlen((const char *)mid);
	memcpy(desc.meter_id, mid, desc.meter_id_len);
	desc.meter_model_len = 3;
	memcpy(desc.meter_model, "ABC", 3);
	desc.channel_count = 1;
	desc.channels[0].id = 1;
	desc.channels[0].address_len = 1;
	desc.channels[0].address[0] = 0x11;
	assert_int_equal(osp_spodus_registry_add(&conc.registry, &desc), OSP_OK);

	osp_spodus_direct_channel_t row = {.direct_id = 200, .meter_id_len = desc.meter_id_len, .channel_id = 1};
	memcpy(row.meter_id, mid, desc.meter_id_len);
	assert_int_equal(osp_spodus_direct_table_add(&conc.direct, &row), OSP_OK);
	osp_spodus_channel_t channel = {.channel_id = 1, .interface_len = 12};
	memcpy(channel.interface, "RS485_1:9600", channel.interface_len);
	assert_int_equal(osp_spodus_channel_list_add(&conc.channels, &channel), OSP_OK);

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	assert_int_equal(osp_spodus_concentrator_register_server(&server, &conc), OSP_OK);

	osp_sec_context_t ssec;
	osp_sec_context_init(&ssec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &ssec);

	g_ivke_server = &server;
	pair.client_transport.ctx = &pair;
	pair.client_transport.send = ivke_loopback_send;
	pair.client_transport.recv = ivke_loopback_recv;

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	assert_int_equal(osp_client_connect(&client, 5000), OSP_OK);

	osp_obis_t ml_obis = osp_spodus_obis_meter_list();
	osp_value_t meter_list;
	assert_int_equal(osp_client_get(&client, 1, &ml_obis, 1, &meter_list), OSP_OK);
	assert_int_equal(meter_list.tag, OSP_TAG_ARRAY);
	assert_int_equal(meter_list.as.array.elements.count, 1);

	osp_obis_t dc_obis = osp_spodus_obis_direct_channel_table();
	osp_value_t direct_table;
	assert_int_equal(osp_client_get(&client, 1, &dc_obis, 1, &direct_table), OSP_OK);
	assert_int_equal(direct_table.tag, OSP_TAG_ARRAY);
	assert_int_equal(direct_table.as.array.elements.count, 1);
	assert_int_equal(direct_table.as.array.elements.items[0].as.structure.elements.items[0].as.uint16.value, 200);

	osp_obis_t channels_obis = osp_spodus_obis_channel_list();
	osp_value_t channels;
	assert_int_equal(osp_client_get(&client, 7, &channels_obis, 2, &channels), OSP_OK);
	assert_int_equal(channels.tag, OSP_TAG_ARRAY);
	assert_int_equal(channels.as.array.elements.count, 1);
	assert_int_equal(channels.as.array.elements.items[0].as.structure.elements.items[0].as.uint8.value, 1);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_registry_meter_list_and_cache),
	    cmocka_unit_test(test_direct_channel_table),
	    cmocka_unit_test(test_channel_list_builds_profile),
	    cmocka_unit_test(test_discovered_meters_build_profile),
	    cmocka_unit_test(test_access_policies_build_value),
	    cmocka_unit_test(test_poll_meter_updates_cache),
	    cmocka_unit_test(test_proxy_forward_roundtrip),
	    cmocka_unit_test(test_concentrator_server_get_objects),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
