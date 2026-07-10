#include "parameter_monitor.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t pm_attrs[] = {1};

static osp_err_t pm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t pm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_parameter_monitor_class(), inst, buf, pm_attrs, 1);
}

static osp_err_t pm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_parameter_monitor_class(), inst, buf, pm_attrs, 1);
}

static const osp_ic_class_t ic_parameter_monitor = {
    .name = "Parameter Monitor",
    .class_id = 65,
    .version = 0,
    .get_attr = pm_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = pm_serialize,
    .deserialize = pm_deserialize,
    .instance_size = sizeof(osp_ic_parameter_monitor_t),
};

const osp_ic_class_t *osp_ic_parameter_monitor_class(void) {
	return &ic_parameter_monitor;
}

void osp_ic_parameter_monitor_init(osp_ic_parameter_monitor_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
