#include "clock.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>

static const uint8_t clock_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

static osp_err_t clock_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_clock_t *c = (const osp_ic_clock_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &c->logical_name);
		case 2:
			*result = osp_val_cosem_datetime(&c->time);
			return OSP_OK;
		case 3:
			*result = osp_val_i32((int32_t)c->timezone_offset);
			return OSP_OK;
		case 4:
			*result = osp_val_u8(c->clock_status);
			return OSP_OK;
		case 5:
			*result = osp_val_cosem_datetime(&c->daylight_savings_begin);
			return OSP_OK;
		case 6:
			*result = osp_val_cosem_datetime(&c->daylight_savings_end);
			return OSP_OK;
		case 7:
			*result = osp_val_i8((int8_t)c->daylight_savings_deviation);
			return OSP_OK;
		case 8:
			*result = osp_val_bool(c->daylight_savings_enabled != 0);
			return OSP_OK;
		case 9:
			*result = osp_val_enum(c->clock_base);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t clock_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_clock_t *c = (osp_ic_clock_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return osp_cosem_datetime_read_value(value, &c->time);
		case 3:
			c->timezone_offset = (uint32_t)osp_get_i32(value);
			return OSP_OK;
		case 4:
			c->clock_status = osp_get_u8(value);
			return OSP_OK;
		case 5:
			return osp_cosem_datetime_read_value(value, &c->daylight_savings_begin);
		case 6:
			return osp_cosem_datetime_read_value(value, &c->daylight_savings_end);
		case 7:
			c->daylight_savings_deviation = (uint8_t)osp_get_i8(value);
			return OSP_OK;
		case 8:
			c->daylight_savings_enabled = osp_get_bool(value) ? 1 : 0;
			return OSP_OK;
		case 9:
			c->clock_base = osp_get_enum(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
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

static osp_err_t clock_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_clock_class(), inst, buf, clock_attrs, 9);
}

static osp_err_t clock_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_clock_class(), inst, buf, clock_attrs, 9);
}

static const osp_ic_class_t ic_clock = {
    .name = "Clock",
    .class_id = 8,
    .version = 0,
    .get_attr = clock_get,
    .set_attr = clock_set,
    .invoke = clock_invoke,
    .serialize = clock_serialize,
    .deserialize = clock_deserialize,
    .instance_size = sizeof(osp_ic_clock_t),
};

const osp_ic_class_t *osp_ic_clock_class(void) {
	return &ic_clock;
}

void osp_ic_clock_init(osp_ic_clock_t *c, osp_obis_t ln) {
	memset(c, 0, sizeof(*c));
	c->logical_name = ln;
	c->daylight_savings_enabled = 1;
}
