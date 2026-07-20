#include "register_activation.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ra_attrs[] = {1, 2, 3};

static osp_err_t ra_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_register_activation_t *a = (const osp_ic_register_activation_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
		case 3:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ra_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	(void)inst;
	(void)value;
	if (attr_id == 2 || attr_id == 3) {
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t ra_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	(void)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id >= 1 && method_id <= 3) {
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t ra_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_register_activation_class(), inst, buf, ra_attrs, 3);
}

static osp_err_t ra_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_register_activation_class(), inst, buf, ra_attrs, 3);
}

static const osp_ic_class_t ic_ra = {
    .name = "Register Activation",
    .class_id = 6,
    .version = 0,
    .get_attr = ra_get,
    .set_attr = ra_set,
    .invoke = ra_invoke,
    .serialize = ra_serialize,
    .deserialize = ra_deserialize,
    .instance_size = sizeof(osp_ic_register_activation_t),
};

const osp_ic_class_t *osp_ic_register_activation_class(void) {
	return &ic_ra;
}

void osp_ic_register_activation_init(osp_ic_register_activation_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
