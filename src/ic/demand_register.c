#include "demand_register.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t dr_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

static osp_value_t dr_time_value(const osp_obis_t *t) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = 6;
	memcpy(v.as.octetstring.data, t, 6);
	return v;
}

static osp_err_t dr_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_demand_register_t *d = (const osp_ic_demand_register_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &d->logical_name);
		case 2:
			*result = d->current_average_value;
			return OSP_OK;
		case 3:
			*result = d->last_average_value;
			return OSP_OK;
		case 4:
			*result = osp_ic_val_scaler_unit(&d->scaler_unit);
			return OSP_OK;
		case 5:
			*result = d->status;
			return OSP_OK;
		case 6:
			*result = dr_time_value(&d->capture_time);
			return OSP_OK;
		case 7:
			*result = dr_time_value(&d->start_time_current);
			return OSP_OK;
		case 8:
			*result = osp_val_u32(d->period);
			return OSP_OK;
		case 9:
			*result = osp_val_u16(d->number_of_periods);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t dr_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_demand_register_t *d = (osp_ic_demand_register_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			d->current_average_value = *value;
			return OSP_OK;
		case 3:
			d->last_average_value = *value;
			return OSP_OK;
		case 4:
			return osp_ic_read_scaler_unit(value, &d->scaler_unit);
		case 5:
			d->status = *value;
			return OSP_OK;
		case 6:
		case 7:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 6) {
				return OSP_ERR_INVALID;
			}
			if (attr_id == 6) {
				memcpy(&d->capture_time, value->as.octetstring.data, 6);
			} else {
				memcpy(&d->start_time_current, value->as.octetstring.data, 6);
			}
			return OSP_OK;
		case 8:
			d->period = osp_get_u32(value);
			return OSP_OK;
		case 9:
			d->number_of_periods = osp_get_u16(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t dr_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_demand_register_t *d = (osp_ic_demand_register_t *)inst;
	(void)param;
	if (method_id == 1) {
		d->current_average_value = osp_val_i32(0);
		d->last_average_value = osp_val_i32(0);
		d->status = osp_val_null();
		if (result) {
			*result = osp_val_null();
		}
		return OSP_OK;
	}
	if (method_id == 2) {
		d->last_average_value = d->current_average_value;
		if (result) {
			*result = osp_val_null();
		}
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t dr_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_demand_register_class(), inst, buf, dr_attrs, 9);
}

static osp_err_t dr_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_demand_register_class(), inst, buf, dr_attrs, 9);
}

static const osp_ic_class_t ic_dr = {
    .name = "Demand Register",
    .class_id = 5,
    .version = 0,
    .get_attr = dr_get,
    .set_attr = dr_set,
    .invoke = dr_invoke,
    .serialize = dr_serialize,
    .deserialize = dr_deserialize,
    .instance_size = sizeof(osp_ic_demand_register_t),
};

const osp_ic_class_t *osp_ic_demand_register_class(void) {
	return &ic_dr;
}

void osp_ic_demand_register_init(osp_ic_demand_register_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
