#include "special_days.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t sd_attrs[] = {1, 2};

static osp_value_t sd_entries_val(const osp_ic_special_days_t *d) {
	OSP_TLS osp_special_days_list_view_t view;
	osp_value_t v = {0};
	uint8_t n = d->entry_count;
	if (n > 32) {
		n = 32;
	}
	if (n == 0) {
		return osp_ic_val_empty_array();
	}
	view.entries = d->entries;
	view.count = n;
	v.tag = OSP_TAG_SPECIAL_DAYS_LIST_REF;
	v.as.ref = &view;
	return v;
}

static osp_err_t sd_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_special_days_t *d = (const osp_ic_special_days_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &d->logical_name);
		case 2:
			*result = sd_entries_val(d);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sd_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	if (attr_id != 2) {
		return OSP_ERR_NOT_FOUND;
	}
	if (!value || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	osp_ic_special_days_t *d = (osp_ic_special_days_t *)inst;
	uint8_t n = value->as.array.elements.count;
	if (n > 32) {
		n = 32;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_value_t *row = &value->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
			return OSP_ERR_INVALID;
		}
		d->entries[i].day_id = osp_get_u32(&row->as.structure.elements.items[0]);
		const osp_value_t *dt = &row->as.structure.elements.items[1];
		if (dt->tag != OSP_TAG_OCTETSTRING || dt->as.octetstring.len < 5) {
			return OSP_ERR_INVALID;
		}
		memcpy(d->entries[i].date, dt->as.octetstring.data, 5);
	}
	d->entry_count = n;
	return OSP_OK;
}

static osp_err_t sd_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_special_days_t *d = (osp_ic_special_days_t *)inst;
	*result = osp_val_null();
	if (method_id == 1) {
		/* insert: optional structure { day_id, date } */
		if (d->entry_count >= 32) {
			return OSP_ERR_INVALID;
		}
		osp_special_day_t *e = &d->entries[d->entry_count];
		memset(e, 0, sizeof(*e));
		if (param && param->tag == OSP_TAG_STRUCTURE && param->as.structure.elements.count >= 2) {
			e->day_id = osp_get_u32(&param->as.structure.elements.items[0]);
			const osp_value_t *dt = &param->as.structure.elements.items[1];
			if (dt->tag == OSP_TAG_OCTETSTRING && dt->as.octetstring.len >= 5) {
				memcpy(e->date, dt->as.octetstring.data, 5);
			}
		}
		d->entry_count++;
		return OSP_OK;
	}
	if (method_id == 2) {
		/* delete: by index (unsigned) or by day_id */
		if (!param) {
			return OSP_ERR_INVALID;
		}
		if (param->tag == OSP_TAG_UNSIGNED) {
			uint8_t idx = param->as.uint8.value;
			if (idx >= d->entry_count) {
				return OSP_ERR_INVALID;
			}
			for (uint8_t i = idx; i + 1 < d->entry_count; i++) {
				d->entries[i] = d->entries[i + 1];
			}
			d->entry_count--;
			return OSP_OK;
		}
		uint32_t day_id = osp_get_u32(param);
		for (uint8_t i = 0; i < d->entry_count; i++) {
			if (d->entries[i].day_id == day_id) {
				for (uint8_t j = i; j + 1 < d->entry_count; j++) {
					d->entries[j] = d->entries[j + 1];
				}
				d->entry_count--;
				return OSP_OK;
			}
		}
		return OSP_ERR_NOT_FOUND;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t sd_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_special_days_class(), inst, buf, sd_attrs, 2);
}

static osp_err_t sd_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_special_days_class(), inst, buf, sd_attrs, 2);
}

static const osp_ic_class_t ic_sd = {
    .name = "Special Days",
    .class_id = 11,
    .version = 0,
    .get_attr = sd_get,
    .set_attr = sd_set,
    .invoke = sd_invoke,
    .serialize = sd_serialize,
    .deserialize = sd_deserialize,
    .instance_size = sizeof(osp_ic_special_days_t),
};

const osp_ic_class_t *osp_ic_special_days_class(void) {
	return &ic_sd;
}

void osp_ic_special_days_init(osp_ic_special_days_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
