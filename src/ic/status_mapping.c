#include "status_mapping.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t sm_attrs[] = {1};

static osp_err_t sm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t sm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_status_mapping_class(), inst, buf, sm_attrs, 1);
}

static osp_err_t sm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_status_mapping_class(), inst, buf, sm_attrs, 1);
}

static const osp_ic_class_t ic_status_mapping = {
    .name = "Status Mapping",
    .class_id = 63,
    .version = 0,
    .get_attr = sm_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = sm_serialize,
    .deserialize = sm_deserialize,
    .instance_size = sizeof(osp_ic_status_mapping_t),
};

const osp_ic_class_t *osp_ic_status_mapping_class(void) {
	return &ic_status_mapping;
}

void osp_ic_status_mapping_init(osp_ic_status_mapping_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
