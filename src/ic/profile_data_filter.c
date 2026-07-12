/**
 * profile_data_filter.c — SPODUS Profile Data Filter (class 8201)
 */

#include "profile_data_filter.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>

static int column_index(const osp_ic_profile_data_filter_t *f, const osp_value_t *capture_object) {
	const osp_value_t *name = NULL;
	if (capture_object && capture_object->tag == OSP_TAG_STRUCTURE) {
		if (capture_object->as.structure.elements.count > 1) {
			name = &capture_object->as.structure.elements.items[1];
		}
	}
	if (!name || name->tag != OSP_TAG_OCTETSTRING || name->as.octetstring.len != 6) {
		return -1;
	}
	for (uint8_t i = 0; i < f->column_count; i++) {
		if (memcmp(name->as.octetstring.data, &f->columns[i], 6) == 0) {
			return (int)i;
		}
	}
	return -1;
}

static bool passes_one(const osp_ic_profile_data_filter_t *f, const osp_value_t *row_fields, uint8_t field_count, const osp_value_t *filter) {
	if (!filter || filter->tag != OSP_TAG_STRUCTURE) {
		return true;
	}
	osp_value_list_t *fl = (osp_value_list_t *)&filter->as.structure.elements;
	if (fl->count < 1) {
		return true;
	}
	int index = column_index(f, &fl->items[0]);
	if (index < 0 || (uint8_t)index >= field_count) {
		return true;
	}
	const osp_value_t *value = &row_fields[index];
	const osp_value_t *from = fl->count > 1 ? &fl->items[1] : NULL;
	const osp_value_t *to = fl->count > 2 ? &fl->items[2] : NULL;
	const osp_value_t *entries = fl->count > 3 ? &fl->items[3] : NULL;

	if (entries && entries->tag == OSP_TAG_ARRAY && entries->as.array.elements.count > 0) {
		for (uint8_t i = 0; i < entries->as.array.elements.count; i++) {
			if (osp_ic_value_eq(value, &entries->as.array.elements.items[i])) {
				return true;
			}
		}
		return false;
	}

	if (from && from->tag != OSP_TAG_NULL) {
		if (osp_ic_value_compare(from, value) > 0) {
			return false;
		}
	}
	if (to && to->tag != OSP_TAG_NULL) {
		if (osp_ic_value_compare(value, to) > 0) {
			return false;
		}
	}
	return true;
}

static bool passes_all(const osp_ic_profile_data_filter_t *f, const osp_value_t *row_fields, uint8_t field_count, const osp_value_t *filters, uint8_t filter_count) {
	for (uint8_t i = 0; i < filter_count; i++) {
		if (!passes_one(f, row_fields, field_count, &filters[i])) {
			return false;
		}
	}
	return true;
}

static osp_err_t project_row(const osp_ic_profile_data_filter_t *f, const osp_value_t *row_fields, uint8_t field_count, const osp_value_t *selected,
                             uint8_t selected_count, osp_value_t *out) {
	out->tag = OSP_TAG_STRUCTURE;
	out->as.structure.elements.count = 0;
	out->as.structure.elements.capacity = OSP_MAX_STRUCT_LEN;
	out->as.structure.elements.items = out->as.structure.elements.items;

	static osp_value_t proj_fields[OSP_MAX_STRUCT_LEN];
	uint8_t n = 0;

	if (selected_count == 0) {
		for (uint8_t i = 0; i < field_count && n < OSP_MAX_STRUCT_LEN; i++) {
			proj_fields[n++] = row_fields[i];
		}
	} else {
		for (uint8_t s = 0; s < selected_count; s++) {
			int idx = column_index(f, &selected[s]);
			if (idx >= 0 && (uint8_t)idx < field_count && n < OSP_MAX_STRUCT_LEN) {
				proj_fields[n++] = row_fields[idx];
			}
		}
	}
	out->as.structure.elements.items = proj_fields;
	out->as.structure.elements.count = n;
	return OSP_OK;
}

static osp_err_t filtered(const osp_ic_profile_data_filter_t *f, const osp_value_t *selected, uint8_t selected_count, const osp_value_t *filters,
                          uint8_t filter_count, osp_ic_profile_data_filter_t *mutable_f, osp_value_t *result) {
	uint8_t n = 0;
	for (uint8_t i = 0; i < f->row_count && n < OSP_MAX_ARRAY_LEN; i++) {
		const osp_ic_row_t *row = &f->rows[i];
		if (!passes_all(f, row->fields, row->field_count, filters, filter_count)) {
			continue;
		}
		project_row(f, row->fields, row->field_count, selected, selected_count, &mutable_f->result_buf[n++]);
	}
	result->tag = OSP_TAG_ARRAY;
	result->as.array.elements.items = mutable_f->result_buf;
	result->as.array.elements.count = n;
	result->as.array.elements.capacity = OSP_MAX_ARRAY_LEN;
	return OSP_OK;
}

static const uint8_t pdf_attrs[] = {1};

static osp_err_t pdf_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_profile_data_filter_t *f = (const osp_ic_profile_data_filter_t *)inst;
	if (attr_id != 1 || !result) {
		return OSP_ERR_NOT_FOUND;
	}
	return osp_ic_get_logical_name(result, &f->logical_name);
}

static osp_err_t pdf_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_profile_data_filter_t *f = (osp_ic_profile_data_filter_t *)inst;
	osp_value_t selected[OSP_MAX_ARRAY_LEN];
	osp_value_t filters[OSP_MAX_ARRAY_LEN];
	uint8_t sel_count = 0;
	uint8_t filt_count = 0;

	if (method_id == 1 || method_id == 2 || method_id == 3 || method_id == 4) {
		if ((method_id != 1) && !param) {
			return OSP_ERR_INVALID;
		}
		if (param && osp_ic_spodus_parse_filter_request(param, selected, OSP_MAX_ARRAY_LEN, &sel_count, filters, OSP_MAX_ARRAY_LEN, &filt_count) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
	}

	switch (method_id) {
		case 1: {
			uint8_t count = 0;
			for (uint8_t i = 0; i < f->row_count; i++) {
				if (passes_all(f, f->rows[i].fields, f->rows[i].field_count, filters, filt_count)) {
					count++;
				}
			}
			if (!result) {
				return OSP_ERR_INVALID;
			}
			*result = osp_val_u8(count);
			return OSP_OK;
		}
		case 2:
		case 3:
			if (!result) {
				return OSP_ERR_INVALID;
			}
			return filtered(f, selected, sel_count, filters, filt_count, f, result);
		case 4: {
			uint8_t w = 0;
			for (uint8_t i = 0; i < f->row_count; i++) {
				if (!passes_all(f, f->rows[i].fields, f->rows[i].field_count, filters, filt_count)) {
					f->rows[w++] = f->rows[i];
				}
			}
			f->row_count = w;
			if (result) {
				*result = osp_val_null();
			}
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pdf_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_profile_data_filter_class(), inst, buf, pdf_attrs, 1);
}

static osp_err_t pdf_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_profile_data_filter_class(), inst, buf, pdf_attrs, 1);
}

static const osp_ic_class_t ic_profile_data_filter = {
    .name = "Profile Data Filter",
    .class_id = 8201,
    .version = 0,
    .get_attr = pdf_get_attr,
    .set_attr = NULL,
    .invoke = pdf_invoke,
    .serialize = pdf_serialize,
    .deserialize = pdf_deserialize,
    .instance_size = sizeof(osp_ic_profile_data_filter_t),
};

const osp_ic_class_t *osp_ic_profile_data_filter_class(void) {
	return &ic_profile_data_filter;
}

void osp_ic_profile_data_filter_init(osp_ic_profile_data_filter_t *f, osp_obis_t ln) {
	if (!f) {
		return;
	}
	memset(f, 0, sizeof(*f));
	f->logical_name = ln;
}

void osp_ic_profile_data_filter_set_columns(osp_ic_profile_data_filter_t *f, const osp_obis_t *columns, uint8_t count) {
	if (!f || !columns) {
		return;
	}
	if (count > OSP_PROFILE_DATA_FILTER_MAX_COLUMNS) {
		count = OSP_PROFILE_DATA_FILTER_MAX_COLUMNS;
	}
	memcpy(f->columns, columns, count * sizeof(osp_obis_t));
	f->column_count = count;
}

void osp_ic_profile_data_filter_set_rows(osp_ic_profile_data_filter_t *f, const osp_ic_row_t *rows, uint8_t count) {
	if (!f || !rows) {
		return;
	}
	if (count > OSP_PROFILE_DATA_FILTER_MAX_ROWS) {
		count = OSP_PROFILE_DATA_FILTER_MAX_ROWS;
	}
	memcpy(f->rows, rows, count * sizeof(osp_ic_row_t));
	f->row_count = count;
}
