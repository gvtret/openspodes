#include "utility_tables.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t ut_attrs[] = {1};

static osp_err_t ut_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t ut_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_utility_tables_class(), inst, buf, ut_attrs, 1);
}

static osp_err_t ut_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_utility_tables_class(), inst, buf, ut_attrs, 1);
}

static const osp_ic_class_t ic_utility_tables = {
    .name = "Utility Tables",
    .class_id = 26,
    .version = 0,
    .get_attr = ut_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = ut_serialize,
    .deserialize = ut_deserialize,
    .instance_size = sizeof(osp_ic_utility_tables_t),
};

const osp_ic_class_t *osp_ic_utility_tables_class(void) {
	return &ic_utility_tables;
}

void osp_ic_utility_tables_init(osp_ic_utility_tables_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
