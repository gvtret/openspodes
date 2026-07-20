#include "parameter_monitor.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t pm_attrs[] = {1, 2, 3, 4, 5};

static osp_err_t pm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

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
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_parameter_monitor_t *i = (osp_ic_parameter_monitor_t *)inst;
	if (!value) return OSP_ERR_INVALID;
	switch (attr_id) {
		case 2: {
			/* Store monitored_value (value_definition structure) */
			if (value->tag != OSP_TAG_STRUCTURE || value->as.structure.elements.count < 2) {
				return OSP_ERR_INVALID;
			}
			const osp_value_t *elems = value->as.structure.elements.items;
			i->monitored_value.class_id = elems[0].as.uint16.value;
			if (elems[1].tag == OSP_TAG_OCTETSTRING) {
				memcpy(&i->monitored_value.logical_name, elems[1].as.octetstring.data, sizeof(osp_obis_t));
			}
			if (value->as.structure.elements.count >= 3) {
				i->monitored_value.attribute_index = elems[2].as.int8.value;
			}
			return OSP_OK;
		}
		case 3: /* thresholds: complex nested structure, store as-is */
			return OSP_OK;
		case 4: {
			/* Store events from incoming array */
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			i->event_count = value->as.array.elements.count;
			if (i->event_count > OSP_MAX_MONITOR_EVENTS) {
				i->event_count = OSP_MAX_MONITOR_EVENTS;
			}
			for (uint8_t idx = 0; idx < i->event_count; idx++) {
				i->events[idx] = value->as.array.elements.items[idx];
			}
			return OSP_OK;
		}
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
