#include "single_action_schedule.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t sas_attrs[] = {1, 2, 3, 4};

static osp_value_t sas_executed_script(const osp_ic_single_action_schedule_t *s) {
	OSP_TLS osp_value_t fields[2];
	osp_value_t v = {0};
	fields[0].tag = OSP_TAG_OCTETSTRING;
	fields[0].as.octetstring.len = 6;
	memcpy(fields[0].as.octetstring.data, &s->script_logical_name, 6);
	fields[1] = osp_val_u16(s->script_selector);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

static osp_value_t sas_execution_time(const osp_ic_single_action_schedule_t *s) {
	OSP_TLS osp_value_t items[OSP_MAX_EXECUTION_TIMES];
	OSP_TLS osp_value_t fields[OSP_MAX_EXECUTION_TIMES][2];
	osp_value_t v = {0};
	uint8_t n = s->execution_time_count;
	if (n > OSP_MAX_EXECUTION_TIMES) {
		n = OSP_MAX_EXECUTION_TIMES;
	}
	for (uint8_t i = 0; i < n; i++) {
		fields[i][0].tag = OSP_TAG_OCTETSTRING;
		fields[i][0].as.octetstring.len = 4;
		memcpy(fields[i][0].as.octetstring.data, s->execution_time[i].time, 4);
		fields[i][1].tag = OSP_TAG_OCTETSTRING;
		fields[i][1].as.octetstring.len = 5;
		memcpy(fields[i][1].as.octetstring.data, s->execution_time[i].date, 5);
		items[i].tag = OSP_TAG_STRUCTURE;
		items[i].as.structure.elements.items = fields[i];
		items[i].as.structure.elements.count = 2;
		items[i].as.structure.elements.capacity = 2;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = OSP_MAX_EXECUTION_TIMES;
	return v;
}

static osp_err_t sas_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_single_action_schedule_t *s = (const osp_ic_single_action_schedule_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			*result = sas_executed_script(s);
			return OSP_OK;
		case 3:
			*result = osp_val_enum(s->schedule_type);
			return OSP_OK;
		case 4:
			*result = sas_execution_time(s);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sas_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_single_action_schedule_t *s = (osp_ic_single_action_schedule_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2: {
			if (value->tag != OSP_TAG_STRUCTURE || value->as.structure.elements.count < 2) {
				return OSP_ERR_INVALID;
			}
			const osp_value_t *ln = &value->as.structure.elements.items[0];
			if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
				return OSP_ERR_INVALID;
			}
			memcpy(&s->script_logical_name, ln->as.octetstring.data, 6);
			s->script_selector = osp_get_u16(&value->as.structure.elements.items[1]);
			return OSP_OK;
		}
		case 3:
			s->schedule_type = osp_get_enum(value);
			return OSP_OK;
		case 4: {
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			uint8_t n = value->as.array.elements.count;
			if (n > OSP_MAX_EXECUTION_TIMES) {
				n = OSP_MAX_EXECUTION_TIMES;
			}
			for (uint8_t i = 0; i < n; i++) {
				const osp_value_t *el = &value->as.array.elements.items[i];
				if (el->tag != OSP_TAG_STRUCTURE || el->as.structure.elements.count < 2) {
					return OSP_ERR_INVALID;
				}
				const osp_value_t *tm = &el->as.structure.elements.items[0];
				const osp_value_t *dt = &el->as.structure.elements.items[1];
				if (tm->tag != OSP_TAG_OCTETSTRING || tm->as.octetstring.len != 4 ||
				    dt->tag != OSP_TAG_OCTETSTRING || dt->as.octetstring.len != 5) {
					return OSP_ERR_INVALID;
				}
				memcpy(s->execution_time[i].time, tm->as.octetstring.data, 4);
				memcpy(s->execution_time[i].date, dt->as.octetstring.data, 5);
			}
			s->execution_time_count = n;
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sas_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_single_action_schedule_class(), inst, buf, sas_attrs, 4);
}

static osp_err_t sas_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_single_action_schedule_class(), inst, buf, sas_attrs, 4);
}

static const osp_ic_class_t ic_sas = {
    .name = "Single Action Schedule",
    .class_id = 22,
    .version = 0,
    .get_attr = sas_get,
    .set_attr = sas_set,
    .invoke = NULL,
    .serialize = sas_serialize,
    .deserialize = sas_deserialize,
    .instance_size = sizeof(osp_ic_single_action_schedule_t),
};

const osp_ic_class_t *osp_ic_single_action_schedule_class(void) {
	return &ic_sas;
}

void osp_ic_single_action_schedule_init(osp_ic_single_action_schedule_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
