#include "register_table.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t rt_attrs[] = {1};

static osp_err_t rt_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t rt_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_register_table_class(), inst, buf, rt_attrs, 1);
}

static osp_err_t rt_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_register_table_class(), inst, buf, rt_attrs, 1);
}

static const osp_ic_class_t ic_register_table = {
    .name = "Register Table",
    .class_id = 61,
    .version = 0,
    .get_attr = rt_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = rt_serialize,
    .deserialize = rt_deserialize,
    .instance_size = sizeof(osp_ic_register_table_t),
};

const osp_ic_class_t *osp_ic_register_table_class(void) {
	return &ic_register_table;
}

void osp_ic_register_table_init(osp_ic_register_table_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
