#include "parameter_monitor.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t pm_attrs[] = {1, 2, 3, 4, 5};

static osp_err_t pm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_parameter_monitor_t *i = (const osp_ic_parameter_monitor_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
		case 2:
			*result = osp_ic_val_value_definition(&i->monitored_value);
			return OSP_OK;
		case 3:
			*result = osp_ic_val_threshold_list(&i->thresholds);
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.items = (osp_value_t *)i->events;
			result->as.array.elements.count = i->event_count;
			result->as.array.elements.capacity = OSP_MAX_MONITOR_EVENTS;
			return OSP_OK;
		case 5:
			*result = osp_val_u32(i->minimal_duration);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pm_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_parameter_monitor_t *i = (osp_ic_parameter_monitor_t *)inst;
	if (!value) return OSP_ERR_INVALID;
	switch (attr_id) {
		case 2: /* monitored_value: accept but don't store (static) */
		case 3: /* thresholds: accept but don't store (static) */
		case 4: /* events: accept but don't store (dynamic) */
			return OSP_OK;
		case 5: /* minimal_duration */
			i->minimal_duration = osp_get_u32(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_parameter_monitor_class(), inst, buf, pm_attrs, 5);
}

static osp_err_t pm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_parameter_monitor_class(), inst, buf, pm_attrs, 5);
}

static const osp_ic_class_t ic_parameter_monitor = {
    .name = "Parameter Monitor",
    .class_id = 65,
    .version = 0,
    .get_attr = pm_get,
    .set_attr = pm_set,
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
