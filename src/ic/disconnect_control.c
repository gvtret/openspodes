#include "disconnect_control.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t dc_attrs[] = {1, 2, 3, 4};

static osp_err_t dc_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_disconnect_control_t *d = (const osp_ic_disconnect_control_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &d->logical_name);
		case 2:
			*result = osp_val_bool(d->output_state != 0);
			return OSP_OK;
		case 3:
			*result = osp_val_u8(d->control_state);
			return OSP_OK;
		case 4:
			*result = osp_val_u8(d->control_model);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t dc_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_disconnect_control_t *d = (osp_ic_disconnect_control_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			d->output_state = osp_get_bool(value) ? 1 : 0;
			return OSP_OK;
		case 3:
			d->control_state = osp_get_u8(value);
			return OSP_OK;
		case 4:
			d->control_model = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t dc_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_disconnect_control_t *d = (osp_ic_disconnect_control_t *)inst;
	(void)param;
	switch (method_id) {
		case 1:
			d->output_state = 0;
			*result = osp_val_null();
			return OSP_OK; /* remote disconnect */
		case 2:
			*result = osp_val_null();
			return OSP_OK; /* remote reconnect */
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t dc_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_disconnect_control_class(), inst, buf, dc_attrs, 4);
}

static osp_err_t dc_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_disconnect_control_class(), inst, buf, dc_attrs, 4);
}

static const osp_ic_class_t ic_dc = {
    .name = "Disconnect Control",
    .class_id = 70,
    .version = 0,
    .get_attr = dc_get,
    .set_attr = dc_set,
    .invoke = dc_invoke,
    .serialize = dc_serialize,
    .deserialize = dc_deserialize,
    .instance_size = sizeof(osp_ic_disconnect_control_t),
};

const osp_ic_class_t *osp_ic_disconnect_control_class(void) {
	return &ic_dc;
}

void osp_ic_disconnect_control_init(osp_ic_disconnect_control_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
