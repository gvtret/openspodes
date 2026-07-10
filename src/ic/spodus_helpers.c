/**
 * spodus_helpers.c — Shared helpers for СПОДУС IC classes 8200/8201
 */

#include "spodus_helpers.h"
#include "../codec/serialize.h"
#include <string.h>

bool osp_ic_value_eq(const osp_value_t *a, const osp_value_t *b) {
	if (!a || !b) {
		return false;
	}
	if (a->tag != b->tag) {
		return false;
	}
	switch (a->tag) {
		case OSP_TAG_NULL:
			return true;
		case OSP_TAG_OCTETSTRING:
			return a->as.octetstring.len == b->as.octetstring.len &&
			       memcmp(a->as.octetstring.data, b->as.octetstring.data, a->as.octetstring.len) == 0;
		case OSP_TAG_UNSIGNED:
			return a->as.uint8.value == b->as.uint8.value;
		case OSP_TAG_LONG_UNSIGNED:
			return a->as.uint16.value == b->as.uint16.value;
		case OSP_TAG_DOUBLE_LONG_UNS:
			return a->as.uint32.value == b->as.uint32.value;
		case OSP_TAG_INTEGER:
			return a->as.int8.value == b->as.int8.value;
		case OSP_TAG_LONG:
			return a->as.int16.value == b->as.int16.value;
		case OSP_TAG_DOUBLE_LONG:
			return a->as.int32.value == b->as.int32.value;
		default:
			return false;
	}
}

int osp_ic_value_compare(const osp_value_t *a, const osp_value_t *b) {
	if (!a || !b) {
		return 0;
	}
	int64_t av = 0;
	int64_t bv = 0;

	switch (a->tag) {
		case OSP_TAG_INTEGER:
			av = a->as.int8.value;
			break;
		case OSP_TAG_LONG:
			av = a->as.int16.value;
			break;
		case OSP_TAG_DOUBLE_LONG:
			av = a->as.int32.value;
			break;
		case OSP_TAG_UNSIGNED:
			av = a->as.uint8.value;
			break;
		case OSP_TAG_LONG_UNSIGNED:
			av = a->as.uint16.value;
			break;
		case OSP_TAG_DOUBLE_LONG_UNS:
			av = a->as.uint32.value;
			break;
		case OSP_TAG_ENUM:
			av = a->as.enum_val.value;
			break;
		case OSP_TAG_OCTETSTRING:
			if (b->tag != OSP_TAG_OCTETSTRING) {
				return 0;
			}
			if (a->as.octetstring.len < b->as.octetstring.len) {
				return -1;
			}
			if (a->as.octetstring.len > b->as.octetstring.len) {
				return 1;
			}
			return memcmp(a->as.octetstring.data, b->as.octetstring.data, a->as.octetstring.len);
		default:
			return 0;
	}

	switch (b->tag) {
		case OSP_TAG_INTEGER:
			bv = b->as.int8.value;
			break;
		case OSP_TAG_LONG:
			bv = b->as.int16.value;
			break;
		case OSP_TAG_DOUBLE_LONG:
			bv = b->as.int32.value;
			break;
		case OSP_TAG_UNSIGNED:
			bv = b->as.uint8.value;
			break;
		case OSP_TAG_LONG_UNSIGNED:
			bv = b->as.uint16.value;
			break;
		case OSP_TAG_DOUBLE_LONG_UNS:
			bv = b->as.uint32.value;
			break;
		case OSP_TAG_ENUM:
			bv = b->as.enum_val.value;
			break;
		default:
			return 0;
	}

	if (av < bv) {
		return -1;
	}
	if (av > bv) {
		return 1;
	}
	return 0;
}

const osp_value_t *osp_ic_row_key(const osp_ic_row_t *row, uint8_t key_index) {
	if (!row || key_index >= row->field_count) {
		return NULL;
	}
	return &row->fields[key_index];
}

static const osp_value_t *structure_field(const osp_value_t *val, uint8_t index) {
	if (!val || val->tag != OSP_TAG_STRUCTURE) {
		return NULL;
	}
	if (index >= val->as.structure.elements.count) {
		return NULL;
	}
	return &val->as.structure.elements.items[index];
}

osp_err_t osp_ic_spodus_parse_entries_list(const osp_value_t *param, osp_value_t *entries, uint8_t capacity, uint8_t *count) {
	if (!param || !entries || !count) {
		return OSP_ERR_INVALID;
	}
	*count = 0;
	const osp_value_t *list = structure_field(param, 1);
	if (!list) {
		return OSP_ERR_INVALID;
	}
	if (list->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = list->as.array.elements.count;
	if (n > capacity) {
		return OSP_ERR_NOMEM;
	}
	for (uint8_t i = 0; i < n; i++) {
		entries[i] = list->as.array.elements.items[i];
	}
	*count = n;
	return OSP_OK;
}

osp_err_t osp_ic_spodus_parse_filter_request(const osp_value_t *param, osp_value_t *selected, uint8_t sel_cap, uint8_t *sel_count, osp_value_t *filters,
                                           uint8_t filt_cap, uint8_t *filt_count) {
	if (!param || !selected || !sel_count || !filters || !filt_count) {
		return OSP_ERR_INVALID;
	}
	*sel_count = 0;
	*filt_count = 0;
	const osp_value_t *sel = structure_field(param, 1);
	const osp_value_t *flt = structure_field(param, 2);
	if (sel && sel->tag == OSP_TAG_ARRAY) {
		uint8_t n = sel->as.array.elements.count;
		if (n > sel_cap) {
			return OSP_ERR_NOMEM;
		}
		for (uint8_t i = 0; i < n; i++) {
			selected[i] = sel->as.array.elements.items[i];
		}
		*sel_count = n;
	}
	if (flt && flt->tag == OSP_TAG_ARRAY) {
		uint8_t n = flt->as.array.elements.count;
		if (n > filt_cap) {
			return OSP_ERR_NOMEM;
		}
		for (uint8_t i = 0; i < n; i++) {
			filters[i] = flt->as.array.elements.items[i];
		}
		*filt_count = n;
	}
	return OSP_OK;
}

osp_err_t osp_ic_row_to_value(const osp_ic_row_t *row, osp_value_t *out) {
	if (!row || !out) {
		return OSP_ERR_INVALID;
	}
	out->tag = OSP_TAG_STRUCTURE;
	out->as.structure.elements.items = (osp_value_t *)row->fields;
	out->as.structure.elements.count = row->field_count;
	out->as.structure.elements.capacity = row->field_count;
	return OSP_OK;
}

osp_err_t osp_ic_value_to_row(const osp_value_t *val, osp_ic_row_t *row) {
	if (!val || !row || val->tag != OSP_TAG_STRUCTURE) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = val->as.structure.elements.count;
	if (n > OSP_IC_ROW_MAX_FIELDS) {
		return OSP_ERR_NOMEM;
	}
	for (uint8_t i = 0; i < n; i++) {
		row->fields[i] = val->as.structure.elements.items[i];
	}
	row->field_count = n;
	return OSP_OK;
}
