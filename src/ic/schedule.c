#include "schedule.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include "../codec/codec.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t sched_attrs[] = {1, 2};

static osp_value_t sched_entries_val(const osp_ic_schedule_t *s) {
	OSP_TLS osp_schedule_list_view_t view;
	osp_value_t v = {0};
	uint8_t n = s->entry_count;
	if (n > OSP_MAX_SCHEDULE_ENTRY) {
		n = OSP_MAX_SCHEDULE_ENTRY;
	}
	if (n == 0) {
		return osp_ic_val_empty_array();
	}
	view.entries = s->entries;
	view.count = n;
	v.tag = OSP_TAG_SCHEDULE_LIST_REF;
	v.as.ref = &view;
	return v;
}

/* ── get_attr ───────────────────────────────────────────────────────────── */

static osp_err_t sched_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_schedule_t *s = (const osp_ic_schedule_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			*result = sched_entries_val(s);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── set_attr ───────────────────────────────────────────────────────────── */

static osp_err_t sched_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	if (attr_id == 2) {
		if (!value || value->tag != OSP_TAG_ARRAY) {
			return OSP_ERR_INVALID;
		}
		uint8_t n = value->as.array.elements.count;
		if (n > OSP_MAX_SCHEDULE_ENTRY) {
			n = OSP_MAX_SCHEDULE_ENTRY;
		}
		for (uint8_t i = 0; i < n; i++) {
			const osp_value_t *row = &value->as.array.elements.items[i];
			osp_schedule_entry_t *e = &s->entries[i];
			memset(e, 0, sizeof(*e));
			if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 3) {
				return OSP_ERR_INVALID;
			}
			const osp_value_t *items = row->as.structure.elements.items;
			uint8_t base = 0;
			if (items[0].tag == OSP_TAG_BOOLEAN) {
				e->enable = items[0].as.boolean.value != 0;
				base = 1;
			} else {
				e->enable = true;
			}
			if (row->as.structure.elements.count < base + 3) {
				return OSP_ERR_INVALID;
			}
			if (items[base].tag == OSP_TAG_OCTETSTRING && items[base].as.octetstring.len >= 4) {
				memcpy(e->start_time, items[base].as.octetstring.data, 4);
			}
			if (items[base + 1].tag == OSP_TAG_OCTETSTRING && items[base + 1].as.octetstring.len >= 4) {
				memcpy(e->end_time, items[base + 1].as.octetstring.data, 4);
			}
			if (items[base + 2].tag == OSP_TAG_ARRAY) {
				uint8_t sn = items[base + 2].as.array.elements.count;
				if (sn > OSP_MAX_SCRIPT_PER_ACTION) {
					sn = OSP_MAX_SCRIPT_PER_ACTION;
				}
				e->script_count = sn;
				for (uint8_t j = 0; j < sn; j++) {
					const osp_value_t *se = &items[base + 2].as.array.elements.items[j];
					if (se->tag == OSP_TAG_STRUCTURE && se->as.structure.elements.count >= 2) {
						e->scripts[j].script_id = osp_get_u32(&se->as.structure.elements.items[0]);
						e->scripts[j].script_selector = osp_get_i32(&se->as.structure.elements.items[1]);
					}
				}
			}
		}
		s->entry_count = n;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

/* ── invoke (methods 1-3 per Blue Book 4.5.3) ───────────────────────────── */

static osp_err_t sched_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	(void)param;
	*result = osp_val_null();

	switch (method_id) {
	case 1: { /* enable/disable: toggle enable flag by index */
		if (!param || param->tag != OSP_TAG_UNSIGNED) return OSP_ERR_INVALID;
		uint8_t idx = param->as.uint8.value;
		if (idx >= s->entry_count) return OSP_ERR_INVALID;
		s->entries[idx].enable = !s->entries[idx].enable;
		return OSP_OK;
	}
	case 2: { /* insert: add a new entry at the end */
		if (s->entry_count >= OSP_MAX_SCHEDULE_ENTRY) return OSP_ERR_INVALID;
		osp_schedule_entry_t *e = &s->entries[s->entry_count];
		memset(e, 0, sizeof(*e));
		e->enable = true;
		if (param && param->tag == OSP_TAG_STRUCTURE && param->as.structure.elements.count >= 3) {
			const osp_value_t *items = param->as.structure.elements.items;
			/* start_time: octet-string(4) */
			if (items[0].tag == OSP_TAG_OCTETSTRING && items[0].as.octetstring.len >= 4) {
				memcpy(e->start_time, items[0].as.octetstring.data, 4);
			}
			/* end_time: octet-string(4) */
			if (items[1].tag == OSP_TAG_OCTETSTRING && items[1].as.octetstring.len >= 4) {
				memcpy(e->end_time, items[1].as.octetstring.data, 4);
			}
			/* scripts: array of structure { script_id, script_selector } */
			if (items[2].tag == OSP_TAG_ARRAY) {
				uint8_t n = items[2].as.array.elements.count;
				if (n > OSP_MAX_SCRIPT_PER_ACTION) n = OSP_MAX_SCRIPT_PER_ACTION;
				e->script_count = n;
				for (uint8_t i = 0; i < n; i++) {
					const osp_value_t *se = &items[2].as.array.elements.items[i];
					if (se->tag == OSP_TAG_STRUCTURE && se->as.structure.elements.count >= 2) {
						e->scripts[i].script_id = osp_get_u32(&se->as.structure.elements.items[0]);
						e->scripts[i].script_selector = osp_get_i32(&se->as.structure.elements.items[1]);
					}
				}
			}
		}
		s->entry_count++;
		return OSP_OK;
	}
	case 3: { /* delete: remove entry by index */
		if (!param || param->tag != OSP_TAG_UNSIGNED) return OSP_ERR_INVALID;
		uint8_t idx = param->as.uint8.value;
		if (idx >= s->entry_count) return OSP_ERR_INVALID;
		for (uint8_t i = idx; i < s->entry_count - 1; i++) {
			memcpy(&s->entries[i], &s->entries[i + 1], sizeof(osp_schedule_entry_t));
		}
		memset(&s->entries[s->entry_count - 1], 0, sizeof(osp_schedule_entry_t));
		s->entry_count--;
		return OSP_OK;
	}
	default:
		return OSP_ERR_UNSUPPORTED;
	}
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
