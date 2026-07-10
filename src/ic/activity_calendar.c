#include "activity_calendar.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t ac_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static osp_err_t ac_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_activity_calendar_t *a = (const osp_ic_activity_calendar_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = a->calendar_name_active_len;
			memcpy(result->as.octetstring.data, a->calendar_name_active, a->calendar_name_active_len);
			return OSP_OK;
		case 3:
		case 4:
		case 5:
		case 7:
		case 8:
		case 9:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 6:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = a->calendar_name_passive_len;
			memcpy(result->as.octetstring.data, a->calendar_name_passive, a->calendar_name_passive_len);
			return OSP_OK;
		case 10:
			result->tag = OSP_TAG_DATETIME;
			result->as.datetime = a->activate_passive_calendar_time;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ac_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_activity_calendar_t *a = (osp_ic_activity_calendar_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			a->calendar_name_active_len = (uint8_t)value->as.octetstring.len;
			memcpy(a->calendar_name_active, value->as.octetstring.data, a->calendar_name_active_len);
			return OSP_OK;
		case 3:
		case 4:
		case 5:
		case 7:
		case 8:
		case 9:
			return OSP_OK;
		case 6:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			a->calendar_name_passive_len = (uint8_t)value->as.octetstring.len;
			memcpy(a->calendar_name_passive, value->as.octetstring.data, a->calendar_name_passive_len);
			return OSP_OK;
		case 10:
			if (value->tag != OSP_TAG_DATETIME) {
				return OSP_ERR_INVALID;
			}
			a->activate_passive_calendar_time = value->as.datetime;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ac_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_activity_calendar_t *a = (osp_ic_activity_calendar_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		a->calendar_name_active_len = a->calendar_name_passive_len;
		memcpy(a->calendar_name_active, a->calendar_name_passive, a->calendar_name_active_len);
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t ac_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_activity_calendar_class(), inst, buf, ac_attrs, 10);
}

static osp_err_t ac_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_activity_calendar_class(), inst, buf, ac_attrs, 10);
}

static const osp_ic_class_t ic_ac = {
    .name = "Activity Calendar",
    .class_id = 20,
    .version = 0,
    .get_attr = ac_get,
    .set_attr = ac_set,
    .invoke = ac_invoke,
    .serialize = ac_serialize,
    .deserialize = ac_deserialize,
    .instance_size = sizeof(osp_ic_activity_calendar_t),
};

const osp_ic_class_t *osp_ic_activity_calendar_class(void) {
	return &ic_ac;
}

void osp_ic_activity_calendar_init(osp_ic_activity_calendar_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
