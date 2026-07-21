/**
 * data.c — Data interface class (class_id = 1, IEC 62056-6-2)
 *
 * Attributes: 1 = logical_name, 2 = value.
 */

#include "data.h"
#include "ic_common.h"
#include "../codec/codec.h"
#include "../codec/ic_serialize.h"
#include "../codec/serialize.h"
#include "../data_hal.h"
#include <string.h>

static osp_err_t data_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_data_t *d = (const osp_ic_data_t *)inst;
	if (!result) {
		return OSP_ERR_NOT_FOUND;
	}
	if (osp_hal_data && osp_hal_data->read) {
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, &d->logical_name, attr_id, result);
		if (r == OSP_OK) {
			return OSP_OK;
		}
		if (r != OSP_ERR_NOT_FOUND) {
			return r;
		}
	}
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &d->logical_name);
		case 2:
			*result = d->value;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t data_set_attr(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_data_t *d = (osp_ic_data_t *)inst;
	if (!value) {
		return OSP_ERR_NOT_FOUND;
	}
	if (osp_hal_data && osp_hal_data->write) {
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, &d->logical_name, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) {
			return r;
		}
	}
	switch (attr_id) {
		case 1:
			return osp_ic_set_logical_name(&d->logical_name, value);
		case 2:
			d->value = *value;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* Method 1: reset — set value to default (null-data) per Blue Book 4.3.1.3.1. */
static osp_err_t data_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_data_t *d = (osp_ic_data_t *)inst;
	(void)param;
	if (method_id != 1) {
		return OSP_ERR_NOT_FOUND;
	}
	if (osp_hal_data && osp_hal_data->execute) {
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, &d->logical_name, method_id, param, result);
		if (r == OSP_OK) {
			return OSP_OK;
		}
		if (r != OSP_ERR_NOT_FOUND) {
			return r;
		}
	}
	d->value = osp_val_null();
	if (result) {
		*result = osp_val_null();
	}
	return OSP_OK;
}

static osp_err_t data_serialize(const void *inst, osp_buf_t *buf) {
	const osp_ic_data_t *d = (const osp_ic_data_t *)inst;
	osp_err_t r = osp_ic_write_object_header(buf, 1, &d->logical_name, 3);
	if (r != OSP_OK) {
		return r;
	}
	return osp_value_write(buf, &d->value);
}

static osp_err_t data_deserialize(void *inst, osp_buf_t *buf) {
	osp_ic_data_t *d = (osp_ic_data_t *)inst;
	uint8_t nf;
	osp_value_t field;
	osp_err_t r = osp_struct_begin_read(buf, &nf);
	if (r != OSP_OK || nf != 3) {
		return OSP_ERR_INVALID;
	}
	r = osp_value_read(buf, &field);
	if (r != OSP_OK || field.tag != OSP_TAG_LONG_UNSIGNED || field.as.uint16.value != 1) {
		return OSP_ERR_INVALID;
	}
	r = osp_obis_read(buf, &d->logical_name);
	if (r != OSP_OK) {
		return r;
	}
	return osp_value_read(buf, &d->value);
}

/* Vtable */
static const osp_ic_class_t data_class = {
    .name = "Data",
    .class_id = 1,
    .version = 0,
    .get_attr = data_get_attr,
    .set_attr = data_set_attr,
    .invoke = data_invoke,
    .serialize = data_serialize,
    .deserialize = data_deserialize,
    .instance_size = sizeof(osp_ic_data_t),
};

const osp_ic_class_t *osp_ic_data_class(void) {
	return &data_class;
}

void osp_ic_data_init(osp_ic_data_t *data, osp_obis_t ln) {
	if (!data) {
		return;
	}
	memset(data, 0, sizeof(*data));
	data->logical_name = ln;
}
