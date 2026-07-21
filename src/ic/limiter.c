#include "limiter.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t lim_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static osp_value_t lim_actions(const osp_limiter_action_t *act) {
	OSP_TLS osp_value_t fields[2];
	OSP_TLS osp_value_t over_fields[2];
	OSP_TLS osp_value_t under_fields[2];
	osp_value_t v = {0};

	over_fields[0].tag = OSP_TAG_OCTETSTRING;
	over_fields[0].as.octetstring.len = 6;
	memcpy(over_fields[0].as.octetstring.data, &act->action_over_threshold.script_logical_name, 6);
	over_fields[1] = osp_val_u16(act->action_over_threshold.script_selector);
	fields[0].tag = OSP_TAG_STRUCTURE;
	fields[0].as.structure.elements.items = over_fields;
	fields[0].as.structure.elements.count = 2;
	fields[0].as.structure.elements.capacity = 2;

	under_fields[0].tag = OSP_TAG_OCTETSTRING;
	under_fields[0].as.octetstring.len = 6;
	memcpy(under_fields[0].as.octetstring.data, &act->action_under_threshold.script_logical_name, 6);
	under_fields[1] = osp_val_u16(act->action_under_threshold.script_selector);
	fields[1].tag = OSP_TAG_STRUCTURE;
	fields[1].as.structure.elements.items = under_fields;
	fields[1].as.structure.elements.count = 2;
	fields[1].as.structure.elements.capacity = 2;

	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

static osp_value_t lim_group_id_list(const osp_ic_limiter_t *l) {
	OSP_TLS osp_value_t items[OSP_MAX_EMERGENCY_GROUP_IDS];
	osp_value_t v = {0};
	uint8_t n = l->emergency_profile_group_id_count;
	if (n > OSP_MAX_EMERGENCY_GROUP_IDS) {
		n = OSP_MAX_EMERGENCY_GROUP_IDS;
	}
	for (uint8_t i = 0; i < n; i++) {
		items[i] = osp_val_u16(l->emergency_profile_group_id_list[i]);
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = OSP_MAX_EMERGENCY_GROUP_IDS;
	return v;
}

static osp_err_t lim_read_action_item(const osp_value_t *val, osp_action_item_t *a) {
	if (!val || !a || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	const osp_value_t *ln = &val->as.structure.elements.items[0];
	if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
		return OSP_ERR_INVALID;
	}
	memcpy(&a->script_logical_name, ln->as.octetstring.data, 6);
	a->script_selector = osp_get_u16(&val->as.structure.elements.items[1]);
	return OSP_OK;
}

static osp_err_t lim_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_limiter_t *l = (const osp_ic_limiter_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &l->logical_name);
		case 2:
			*result = osp_ic_val_value_definition(&l->monitored_value);
			return OSP_OK;
		case 3:
			*result = l->threshold_active;
			return OSP_OK;
		case 4:
			*result = l->threshold_normal;
			return OSP_OK;
		case 5:
			*result = l->threshold_emergency;
			return OSP_OK;
		case 6:
			*result = osp_val_u32(l->min_over_threshold_duration);
			return OSP_OK;
		case 7:
			*result = osp_val_u32(l->min_under_threshold_duration);
			return OSP_OK;
		case 8:
			*result = osp_ic_val_emergency_profile(&l->emergency_profile);
			return OSP_OK;
		case 9:
			*result = lim_group_id_list(l);
			return OSP_OK;
		case 10:
			*result = osp_val_bool(l->emergency_profile_active);
			return OSP_OK;
		case 11:
			*result = lim_actions(&l->actions);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lim_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_limiter_t *l = (osp_ic_limiter_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return osp_ic_read_value_definition(value, &l->monitored_value);
		case 3:
			l->threshold_active = *value;
			return OSP_OK;
		case 4:
			l->threshold_normal = *value;
			return OSP_OK;
		case 5:
			l->threshold_emergency = *value;
			return OSP_OK;
		case 6:
			l->min_over_threshold_duration = osp_get_u32(value);
			return OSP_OK;
		case 7:
			l->min_under_threshold_duration = osp_get_u32(value);
			return OSP_OK;
		case 8:
			return osp_ic_read_emergency_profile(value, &l->emergency_profile);
		case 9: {
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			uint8_t n = value->as.array.elements.count;
			if (n > OSP_MAX_EMERGENCY_GROUP_IDS) {
				n = OSP_MAX_EMERGENCY_GROUP_IDS;
			}
			for (uint8_t i = 0; i < n; i++) {
				l->emergency_profile_group_id_list[i] = osp_get_u16(&value->as.array.elements.items[i]);
			}
			l->emergency_profile_group_id_count = n;
			return OSP_OK;
		}
		case 10:
			l->emergency_profile_active = osp_get_bool(value);
			return OSP_OK;
		case 11: {
			if (value->tag != OSP_TAG_STRUCTURE || value->as.structure.elements.count < 2) {
				return OSP_ERR_INVALID;
			}
			osp_err_t r = lim_read_action_item(&value->as.structure.elements.items[0],
			                                   &l->actions.action_over_threshold);
			if (r != OSP_OK) {
				return r;
			}
			return lim_read_action_item(&value->as.structure.elements.items[1],
			                            &l->actions.action_under_threshold);
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lim_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	(void)inst;
	(void)param;
	(void)method_id;
	if (result) {
		*result = osp_val_null();
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t lim_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_limiter_class(), inst, buf, lim_attrs, 11);
}

static osp_err_t lim_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_limiter_class(), inst, buf, lim_attrs, 11);
}

static const osp_ic_class_t ic_lim = {
    .name = "Limiter",
    .class_id = 71,
    .version = 0,
    .get_attr = lim_get,
    .set_attr = lim_set,
    .invoke = lim_invoke,
    .serialize = lim_serialize,
    .deserialize = lim_deserialize,
    .instance_size = sizeof(osp_ic_limiter_t),
};

const osp_ic_class_t *osp_ic_limiter_class(void) {
	return &ic_lim;
}

void osp_ic_limiter_init(osp_ic_limiter_t *l, osp_obis_t ln) {
	memset(l, 0, sizeof(*l));
	l->logical_name = ln;
	l->threshold_active = osp_val_null();
	l->threshold_normal = osp_val_null();
	l->threshold_emergency = osp_val_null();
}
