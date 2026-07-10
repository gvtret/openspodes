#include "register_monitor.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t rm_attrs[] = {1, 2, 3};

static osp_err_t rm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_register_monitor_t *m = (const osp_ic_register_monitor_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &m->logical_name);
		case 2:
			*result = osp_ic_val_value_definition(&m->monitored_value);
			return OSP_OK;
		case 3:
			*result = osp_ic_val_threshold_list(&m->thresholds);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t rm_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_register_monitor_t *m = (osp_ic_register_monitor_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return osp_ic_read_value_definition(value, &m->monitored_value);
		case 3:
			return osp_ic_read_threshold_list(value, &m->thresholds);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t rm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_register_monitor_class(), inst, buf, rm_attrs, 3);
}

static osp_err_t rm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_register_monitor_class(), inst, buf, rm_attrs, 3);
}

static const osp_ic_class_t ic_rm = {
    .name = "Register Monitor",
    .class_id = 21,
    .version = 0,
    .get_attr = rm_get,
    .set_attr = rm_set,
    .invoke = NULL,
    .serialize = rm_serialize,
    .deserialize = rm_deserialize,
    .instance_size = sizeof(osp_ic_register_monitor_t),
};

const osp_ic_class_t *osp_ic_register_monitor_class(void) {
	return &ic_rm;
}

void osp_ic_register_monitor_init(osp_ic_register_monitor_t *m, osp_obis_t ln) {
	memset(m, 0, sizeof(*m));
	m->logical_name = ln;
}
