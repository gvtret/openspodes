#include "single_action_schedule.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t sas_attrs[] = {1, 2, 3, 4};

static osp_value_t sas_time(const uint8_t *t) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = 4;
	memcpy(v.as.octetstring.data, t, 4);
	return v;
}

static osp_err_t sas_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_single_action_schedule_t *s = (const osp_ic_single_action_schedule_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			*result = osp_val_u32(s->executed_script_id);
			return OSP_OK;
		case 3:
			*result = osp_val_enum(s->schedule_type);
			return OSP_OK;
		case 4:
			*result = sas_time(s->execution_time);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sas_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_single_action_schedule_t *s = (osp_ic_single_action_schedule_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			s->executed_script_id = osp_get_u32(value);
			return OSP_OK;
		case 3:
			s->schedule_type = osp_get_enum(value);
			return OSP_OK;
		case 4:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 4) {
				return OSP_ERR_INVALID;
			}
			memcpy(s->execution_time, value->as.octetstring.data, 4);
			return OSP_OK;
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
