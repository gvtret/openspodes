/**
 * data.c — Data interface class (class_id = 1)
 *
 * Minimal IC proving the vtable + osp_value_t architecture works.
 */

#include "data.h"
#include <string.h>

/* Attribute: value (get/set) */
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

/* Vtable */
static const osp_ic_class_t data_class = {
    .name = "Data",
    .class_id = 1,
    .version = 0,
    .get_attr = data_get_attr,
    .set_attr = data_set_attr,
    .invoke = NULL,
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
