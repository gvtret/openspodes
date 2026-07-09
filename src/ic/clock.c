#include "clock.h"
#include <string.h>

static osp_err_t clock_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_clock_t *c = (const osp_ic_clock_t *)inst;
	switch (attr_id) {
		case 2:
			result->tag = OSP_TAG_DATETIME;
			result->as.datetime = c->time;
			return OSP_OK;
		case 3:
			*result = osp_val_u32(c->timezone_offset);
			return OSP_OK;
		case 4:
			*result = osp_val_u8(c->clock_status);
			return OSP_OK;
		case 5:
			*result = osp_val_u32(c->daylight_savings_begin);
			return OSP_OK;
		case 6:
			*result = osp_val_u32(c->daylight_savings_end);
			return OSP_OK;
		case 7:
			*result = osp_val_u8(c->daylight_savings_deviation);
			return OSP_OK;
		case 8:
			*result = osp_val_bool(c->daylight_savings_enabled != 0);
			return OSP_OK;
		case 9:
			*result = osp_val_enum(c->clock_status);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t clock_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_clock_t *c = (osp_ic_clock_t *)inst;
	if (attr_id == 2 && value && value->tag == OSP_TAG_DATETIME) {
		c->time = value->as.datetime;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t clock_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	switch (method_id) {
		case 1: /* adjust_to_quarter */
			*result = osp_val_null();
			return OSP_OK;
		case 2: /* measuring_period */
			*result = osp_val_null();
			return OSP_OK;
		case 3: /* minute */
			*result = osp_val_null();
			return OSP_OK;
		case 4: /* preset_time */
			*result = osp_val_null();
			return OSP_OK;
		case 5: /* preset_adjusting_time */
			*result = osp_val_null();
			return OSP_OK;
		case 6: /* shift_time */
			*result = osp_val_null();
			return OSP_OK;
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static const osp_ic_class_t ic_clock = {
    .name = "Clock", .class_id = 8, .version = 0, .get_attr = clock_get, .set_attr = clock_set, .invoke = clock_invoke, .instance_size = sizeof(osp_ic_clock_t)
};

const osp_ic_class_t *osp_ic_clock_class(void) {
	return &ic_clock;
}

void osp_ic_clock_init(osp_ic_clock_t *c, osp_obis_t ln) {
	memset(c, 0, sizeof(*c));
	c->logical_name = ln;
	c->daylight_savings_enabled = 1;
}
