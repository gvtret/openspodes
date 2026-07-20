#include "extended_register.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t er_attrs[] = {1, 2, 3, 4, 5};

static osp_value_t er_capture_time_value(const osp_ic_ext_register_t *r) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = 6;
	memcpy(v.as.octetstring.data, &r->capture_time, 6);
	return v;
}

static osp_err_t er_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_ext_register_t *r = (const osp_ic_ext_register_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &r->logical_name);
		case 2:
			*result = r->value;
			return OSP_OK;
		case 3:
			*result = osp_ic_val_scaler_unit(&r->scaler_unit);
			return OSP_OK;
		case 4:
			*result = r->status;
			return OSP_OK;
		case 5:
			*result = er_capture_time_value(r);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t er_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_ext_register_t *r = (osp_ic_ext_register_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			r->value = *value;
			return OSP_OK;
		case 3:
			return osp_ic_read_scaler_unit(value, &r->scaler_unit);
		case 4:
			r->status = *value;
			return OSP_OK;
		case 5:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 6) {
				return OSP_ERR_INVALID;
			}
			memcpy(&r->capture_time, value->as.octetstring.data, 6);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t er_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_ext_register_t *r = (osp_ic_ext_register_t *)inst;
	(void)param;
	if (method_id == 1) {
		r->value = osp_val_i32(0);
		r->status = osp_val_null();
		if (result) {
			*result = osp_val_null();
		}
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t er_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_ext_register_class(), inst, buf, er_attrs, 5);
}

static osp_err_t er_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_ext_register_class(), inst, buf, er_attrs, 5);
}

static const osp_ic_class_t ic_ext_reg = {
    .name = "Extended Register",
    .class_id = 4,
    .version = 0,
    .get_attr = er_get,
    .set_attr = er_set,
    .invoke = er_invoke,
    .serialize = er_serialize,
    .deserialize = er_deserialize,
    .instance_size = sizeof(osp_ic_ext_register_t),
};

const osp_ic_class_t *osp_ic_ext_register_class(void) {
	return &ic_ext_reg;
}

void osp_ic_ext_register_init(osp_ic_ext_register_t *r, osp_obis_t ln) {
	memset(r, 0, sizeof(*r));
	r->logical_name = ln;
}
