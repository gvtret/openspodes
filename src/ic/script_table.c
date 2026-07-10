#include "script_table.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t st_attrs[] = {1, 2};

static osp_err_t st_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_script_table_t *t = (const osp_ic_script_table_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &t->logical_name);
		case 2:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t st_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	(void)inst;
	(void)value;
	if (attr_id == 2) {
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t st_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();
	return (method_id == 1) ? OSP_OK : OSP_ERR_UNSUPPORTED;
}

static osp_err_t st_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_script_table_class(), inst, buf, st_attrs, 2);
}

static osp_err_t st_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_script_table_class(), inst, buf, st_attrs, 2);
}

static const osp_ic_class_t ic_st = {
    .name = "Script Table",
    .class_id = 9,
    .version = 0,
    .get_attr = st_get,
    .set_attr = st_set,
    .invoke = st_invoke,
    .serialize = st_serialize,
    .deserialize = st_deserialize,
    .instance_size = sizeof(osp_ic_script_table_t),
};

const osp_ic_class_t *osp_ic_script_table_class(void) {
	return &ic_st;
}

void osp_ic_script_table_init(osp_ic_script_table_t *t, osp_obis_t ln) {
	memset(t, 0, sizeof(*t));
	t->logical_name = ln;
}
