#include "spodus_data.h"
#include "concentrator.h"
#include "spodus_obis.h"
#include "../codec/ic_serialize.h"
#include "../codec/serialize.h"
#include "../server/server.h"
#include <string.h>

static osp_err_t spodus_data_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_spodus_data_t *d = (const osp_ic_spodus_data_t *)inst;
	if (attr_id != 1 || !result || !d->conc) {
		return OSP_ERR_NOT_FOUND;
	}
	switch (d->kind) {
		case OSP_SPODUS_DATA_METER_LIST:
			return osp_spodus_registry_build_meter_list(&d->conc->registry, result);
		case OSP_SPODUS_DATA_DIRECT_TABLE:
			return osp_spodus_direct_table_build_value(&d->conc->direct, result);
		case OSP_SPODUS_DATA_ACCESS_POLICIES:
			return osp_spodus_access_policies_build_value(&d->conc->access_policies, result);
		case OSP_SPODUS_DATA_EXCHANGE_TASKS:
			return osp_spodus_exchange_tasks_build_value(&d->conc->exchange_tasks, result);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t spodus_data_set_attr(void *inst, uint8_t attr_id, const osp_value_t *value) {
	(void)inst;
	(void)value;
	if (attr_id != 1) {
		return OSP_ERR_NOT_FOUND;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t spodus_data_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)param;
	if (method_id != 1 || !result) {
		return OSP_ERR_NOT_FOUND;
	}
	return spodus_data_get_attr(inst, 1, result);
}

static osp_err_t spodus_data_serialize(const void *inst, osp_buf_t *buf) {
	const osp_ic_spodus_data_t *d = (const osp_ic_spodus_data_t *)inst;
	osp_value_t value;
	osp_err_t r = spodus_data_get_attr(d, 1, &value);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_ic_write_object_header(buf, 1, &d->logical_name, 3);
	if (r != OSP_OK) {
		return r;
	}
	return osp_value_write(buf, &value);
}

static osp_err_t spodus_data_deserialize(void *inst, osp_buf_t *buf) {
	osp_ic_spodus_data_t *d = (osp_ic_spodus_data_t *)inst;
	uint8_t nf;
	osp_value_t field;
	osp_err_t r = osp_struct_begin_read(buf, &nf);
	if (r != OSP_OK || nf != 3) {
		return OSP_ERR_INVALID;
	}
	r = osp_value_read(buf, &field);
	if (r != OSP_OK || field.tag != OSP_TAG_LONG_UNSIGNED || field.as.uint16.value != 1) {
		return OSP_ERR_INVALID;
	}
	r = osp_obis_read(buf, &d->logical_name);
	if (r != OSP_OK) {
		return r;
	}
	(void)osp_value_read(buf, &field);
	return OSP_OK;
}

static const osp_ic_class_t spodus_data_class = {
    .name = "SpodusData",
    .class_id = 1,
    .version = 0,
    .get_attr = spodus_data_get_attr,
    .set_attr = spodus_data_set_attr,
    .invoke = spodus_data_invoke,
    .serialize = spodus_data_serialize,
    .deserialize = spodus_data_deserialize,
    .instance_size = sizeof(osp_ic_spodus_data_t),
};

void osp_ic_spodus_data_init(osp_ic_spodus_data_t *obj, osp_obis_t ln, osp_spodus_concentrator_t *conc, osp_spodus_data_kind_t kind) {
	if (!obj) {
		return;
	}
	memset(obj, 0, sizeof(*obj));
	obj->logical_name = ln;
	obj->conc = conc;
	obj->kind = kind;
}

const osp_ic_class_t *osp_ic_spodus_data_class(void) {
	return &spodus_data_class;
}

osp_err_t osp_spodus_concentrator_register_server(osp_server_t *server, osp_spodus_concentrator_t *conc) {
	if (!server || !conc) {
		return OSP_ERR_INVALID;
	}
	osp_ic_spodus_data_init(&conc->server_objects.meter_list, osp_spodus_obis_meter_list(), conc, OSP_SPODUS_DATA_METER_LIST);
	osp_ic_spodus_data_init(&conc->server_objects.direct_table, osp_spodus_obis_direct_channel_table(), conc, OSP_SPODUS_DATA_DIRECT_TABLE);
	osp_ic_spodus_data_init(&conc->server_objects.access_policies, osp_spodus_obis_access_policies(), conc, OSP_SPODUS_DATA_ACCESS_POLICIES);
	osp_ic_spodus_data_init(&conc->server_objects.exchange_tasks, osp_spodus_obis_exchange_tasks(), conc, OSP_SPODUS_DATA_EXCHANGE_TASKS);
	osp_err_t r = osp_spodus_channel_list_build_profile(&conc->channels, &conc->server_objects.channel_list);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_spodus_discovered_list_build_profile(&conc->discovered, &conc->server_objects.discovered_meters);
	if (r != OSP_OK) {
		return r;
	}

	r = osp_server_register(server, osp_ic_spodus_data_class(), &conc->server_objects.meter_list);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_server_register(server, osp_ic_spodus_data_class(), &conc->server_objects.direct_table);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_server_register(server, osp_ic_spodus_data_class(), &conc->server_objects.access_policies);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_server_register(server, osp_ic_spodus_data_class(), &conc->server_objects.exchange_tasks);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_server_register(server, osp_ic_profile_generic_class(), &conc->server_objects.channel_list);
	if (r != OSP_OK) {
		return r;
	}
	return osp_server_register(server, osp_ic_profile_generic_class(), &conc->server_objects.discovered_meters);
}
