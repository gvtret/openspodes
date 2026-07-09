#include "limiter.h"
#include <string.h>

static osp_err_t lim_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_limiter_t *l = (const osp_ic_limiter_t *)inst;
	switch (attr_id) {
		case 5:
			*result = osp_val_u32(l->min_over_threshold_duration);
			return OSP_OK;
		case 6:
			*result = osp_val_u32(l->min_under_threshold_duration);
			return OSP_OK;
		case 9:
			*result = osp_val_bool(l->emergency_profile_active);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lim_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_limiter_t *l = (osp_ic_limiter_t *)inst;
	if (attr_id == 9 && value) {
		l->emergency_profile_active = osp_get_bool(value);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t lim_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();
	return (method_id == 1) ? OSP_OK : OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_lim = {
    .name = "Limiter", .class_id = 71, .version = 0, .get_attr = lim_get, .set_attr = lim_set, .invoke = lim_invoke, .instance_size = sizeof(osp_ic_limiter_t)
};

const osp_ic_class_t *osp_ic_limiter_class(void) {
	return &ic_lim;
}

void osp_ic_limiter_init(osp_ic_limiter_t *l, osp_obis_t ln) {
	memset(l, 0, sizeof(*l));
	l->logical_name = ln;
}
