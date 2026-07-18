/**
 * data.c — Data interface class (class_id = 1)
 *
 * Minimal IC proving the vtable + osp_value_t architecture works.
 */

#include "data.h"
#include "../codec/codec.h"
#include "../codec/ic_serialize.h"
#include "../codec/serialize.h"
#include <string.h>

/* Attribute: value (get/set) — attr_id=1 returns value per implementation convention */
static osp_err_t data_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_data_t *d = (const osp_ic_data_t *)inst;
	if (attr_id != 1 || !result) {
		return OSP_ERR_NOT_FOUND;
	}
	*result = d->value;
	return OSP_OK;
}

static osp_err_t data_set_attr(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_data_t *d = (osp_ic_data_t *)inst;
	if (attr_id != 1 || !value) {
		return OSP_ERR_NOT_FOUND;
	}
	d->value = *value;
	return OSP_OK;
}

/* Method 1: return attribute value (used by stack integration tests). */
static osp_err_t data_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	const osp_ic_data_t *d = (const osp_ic_data_t *)inst;
	(void)param;
	if (method_id != 1 || !result) {
		return OSP_ERR_NOT_FOUND;
	}
	*result = d->value;
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
