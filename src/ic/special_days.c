#include "special_days.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t sd_attrs[] = {1, 2};

static osp_err_t sd_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_special_days_t *d = (const osp_ic_special_days_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &d->logical_name);
		case 2:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sd_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	(void)inst;
	(void)value;
	if (attr_id == 2) {
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t sd_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_special_days_t *d = (osp_ic_special_days_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		if (d->entry_count < 32) {
			d->entry_count++;
		}
		return OSP_OK;
	}
	if (method_id == 2) {
		if (d->entry_count > 0) {
			d->entry_count--;
		}
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t sd_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_special_days_class(), inst, buf, sd_attrs, 2);
}

static osp_err_t sd_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_special_days_class(), inst, buf, sd_attrs, 2);
}

static const osp_ic_class_t ic_sd = {
    .name = "Special Days",
    .class_id = 11,
    .version = 0,
    .get_attr = sd_get,
    .set_attr = sd_set,
    .invoke = sd_invoke,
    .serialize = sd_serialize,
    .deserialize = sd_deserialize,
    .instance_size = sizeof(osp_ic_special_days_t),
};

const osp_ic_class_t *osp_ic_special_days_class(void) {
	return &ic_sd;
}

void osp_ic_special_days_init(osp_ic_special_days_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
