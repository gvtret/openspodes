#include "schedule.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t sched_attrs[] = {1, 2};

static osp_err_t sched_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_schedule_t *s = (const osp_ic_schedule_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sched_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	(void)inst;
	(void)value;
	if (attr_id == 2) {
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t sched_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		if (s->entry_count > 0) {
			s->entry_count--;
		}
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t sched_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_schedule_class(), inst, buf, sched_attrs, 2);
}

static osp_err_t sched_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_schedule_class(), inst, buf, sched_attrs, 2);
}

static const osp_ic_class_t ic_sched = {
    .name = "Schedule",
    .class_id = 10,
    .version = 0,
    .get_attr = sched_get,
    .set_attr = sched_set,
    .invoke = sched_invoke,
    .serialize = sched_serialize,
    .deserialize = sched_deserialize,
    .instance_size = sizeof(osp_ic_schedule_t),
};

const osp_ic_class_t *osp_ic_schedule_class(void) {
	return &ic_sched;
}

void osp_ic_schedule_init(osp_ic_schedule_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
