#include "limiter.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t lim_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static osp_err_t lim_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_limiter_t *l = (const osp_ic_limiter_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &l->logical_name);
		case 2:
			*result = osp_ic_val_value_definition(&l->monitored_value);
			return OSP_OK;
		case 3:
			*result = osp_ic_val_threshold(&l->threshold_active);
			return OSP_OK;
		case 4:
			*result = osp_ic_val_threshold(&l->threshold_normal);
			return OSP_OK;
		case 5:
			*result = osp_ic_val_threshold(&l->threshold_emergency);
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
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 10:
			*result = osp_val_bool(l->emergency_profile_active);
			return OSP_OK;
		case 11:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lim_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_limiter_t *l = (osp_ic_limiter_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return osp_ic_read_value_definition(value, &l->monitored_value);
		case 3:
			return osp_ic_read_threshold(value, &l->threshold_active);
		case 4:
			return osp_ic_read_threshold(value, &l->threshold_normal);
		case 5:
			return osp_ic_read_threshold(value, &l->threshold_emergency);
		case 6:
			l->min_over_threshold_duration = osp_get_u32(value);
			return OSP_OK;
		case 7:
			l->min_under_threshold_duration = osp_get_u32(value);
			return OSP_OK;
		case 8:
			return osp_ic_read_emergency_profile(value, &l->emergency_profile);
		case 9:
		case 11:
			return OSP_OK;
		case 10:
			l->emergency_profile_active = osp_get_bool(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lim_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
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
}
