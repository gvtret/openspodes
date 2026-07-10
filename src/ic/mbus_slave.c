#include "mbus_slave.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t ms_attrs[] = {1};

static osp_err_t ms_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t ms_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_mbus_slave_class(), inst, buf, ms_attrs, 1);
}

static osp_err_t ms_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_mbus_slave_class(), inst, buf, ms_attrs, 1);
}

static const osp_ic_class_t ic_mbus_slave = {
    .name = "MBus Slave Port Setup",
    .class_id = 76,
    .version = 0,
    .get_attr = ms_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = ms_serialize,
    .deserialize = ms_deserialize,
    .instance_size = sizeof(osp_ic_mbus_slave_t),
};

const osp_ic_class_t *osp_ic_mbus_slave_class(void) {
	return &ic_mbus_slave;
}

void osp_ic_mbus_slave_init(osp_ic_mbus_slave_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
