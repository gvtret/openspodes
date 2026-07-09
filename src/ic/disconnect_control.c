#include "disconnect_control.h"
#include <string.h>

static osp_err_t dc_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_disconnect_control_t *d = (const osp_ic_disconnect_control_t *)inst;
	switch (attr_id) {
		case 2:
			*result = osp_val_u8(d->output_state);
			return OSP_OK;
		case 3:
			*result = osp_val_u8(d->control_model);
			return OSP_OK;
		case 6:
			*result = osp_val_u32(d->power_failure_counter);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t dc_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_disconnect_control_t *d = (osp_ic_disconnect_control_t *)inst;
	if (attr_id == 2 && value) {
		d->output_state = osp_get_u8(value);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t dc_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
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

static const osp_ic_class_t ic_dc = {
    .name = "Disconnect Control",
    .class_id = 70,
    .version = 0,
    .get_attr = dc_get,
    .set_attr = dc_set,
    .invoke = dc_invoke,
    .instance_size = sizeof(osp_ic_disconnect_control_t)
};

const osp_ic_class_t *osp_ic_disconnect_control_class(void) {
	return &ic_dc;
}

void osp_ic_disconnect_control_init(osp_ic_disconnect_control_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
