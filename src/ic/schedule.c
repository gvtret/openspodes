#include "schedule.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include "../codec/codec.h"
#include <string.h>

static const uint8_t sched_attrs[] = {1, 2};

/* ── get_attr ───────────────────────────────────────────────────────────── */

static osp_err_t sched_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_schedule_t *s = (const osp_ic_schedule_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			/* Return schedule entries as array */
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.count = s->entry_count;
			result->as.array.elements.capacity = OSP_MAX_SCHEDULE_ENTRY;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── set_attr ───────────────────────────────────────────────────────────── */

static osp_err_t sched_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	if (attr_id == 2) {
		if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
		/* Store entry count from the array */
		s->entry_count = value->as.array.elements.count;
		if (s->entry_count > OSP_MAX_SCHEDULE_ENTRY) {
			s->entry_count = OSP_MAX_SCHEDULE_ENTRY;
		}
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

/* ── invoke (method 1: remove_entry) ────────────────────────────────────── */

static osp_err_t sched_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	(void)param;
	*result = osp_val_null();

	if (method_id == 1) {
		/* remove_entry: remove the first entry (or by index if param provided) */
		if (s->entry_count == 0) return OSP_ERR_NOT_FOUND;

		uint8_t remove_idx = 0; /* default: remove first */
		if (param && param->tag == OSP_TAG_UNSIGNED) {
			remove_idx = param->as.uint8.value;
			if (remove_idx >= s->entry_count) return OSP_ERR_INVALID;
		}

		/* Shift entries down */
		for (uint8_t i = remove_idx; i < s->entry_count - 1; i++) {
			memcpy(&s->entries[i], &s->entries[i + 1], sizeof(osp_schedule_entry_t));
		}
		memset(&s->entries[s->entry_count - 1], 0, sizeof(osp_schedule_entry_t));
		s->entry_count--;
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

/* ── serialize ──────────────────────────────────────────────────────────── */

static osp_err_t sched_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_schedule_class(), inst, buf, sched_attrs, 2);
}

/* ── deserialize ────────────────────────────────────────────────────────── */

static osp_err_t sched_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_schedule_class(), inst, buf, sched_attrs, 2);
}

/* ── class definition ───────────────────────────────────────────────────── */

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
