/**
 * table_manager.c — SPODUS Table Manager (class 8200, STO 34.01-5.1-013-2023 §7.2)
 */

#include "table_manager.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>

static int find_row_index(const osp_ic_table_manager_t *tm, const osp_value_t *key) {
	for (uint8_t i = 0; i < tm->row_count; i++) {
		const osp_value_t *rk = osp_ic_row_key(&tm->rows[i], tm->key_index);
		if (rk && osp_ic_value_eq(rk, key)) {
			return (int)i;
		}
	}
	return -1;
}

static osp_err_t tm_add_update(osp_ic_table_manager_t *tm, const osp_value_t *entries, uint8_t count) {
	for (uint8_t i = 0; i < count; i++) {
		osp_ic_row_t row;
		osp_err_t r = osp_ic_value_to_row(&entries[i], &row);
		if (r != OSP_OK) {
			return r;
		}
		const osp_value_t *key = osp_ic_row_key(&row, tm->key_index);
		if (!key) {
			return OSP_ERR_INVALID;
		}
		int idx = find_row_index(tm, key);
		if (idx >= 0) {
			tm->rows[idx] = row;
		} else if (tm->row_count < OSP_TABLE_MANAGER_MAX_ROWS) {
			tm->rows[tm->row_count++] = row;
		} else {
			return OSP_ERR_NOMEM;
		}
	}
	return OSP_OK;
}

static osp_err_t tm_remove(osp_ic_table_manager_t *tm, const osp_value_t *keys, uint8_t count) {
	if (count == 0) {
		tm->row_count = 0;
		return OSP_OK;
	}
	uint8_t w = 0;
	for (uint8_t i = 0; i < tm->row_count; i++) {
		bool drop = false;
		const osp_value_t *rk = osp_ic_row_key(&tm->rows[i], tm->key_index);
		for (uint8_t k = 0; k < count; k++) {
			if (rk && osp_ic_value_eq(rk, &keys[k])) {
				drop = true;
				break;
			}
		}
		if (!drop) {
			tm->rows[w++] = tm->rows[i];
		}
	}
	tm->row_count = w;
	return OSP_OK;
}

static osp_err_t tm_retrieve(osp_ic_table_manager_t *tm, const osp_value_t *keys, uint8_t key_count, osp_value_t *result) {
	if (!result) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = 0;

	if (key_count == 0) {
		for (uint8_t i = 0; i < tm->row_count && n < OSP_MAX_ARRAY_LEN; i++) {
			osp_ic_row_to_value(&tm->rows[i], &tm->retrieve_buf[n++]);
		}
	} else {
		for (uint8_t k = 0; k < key_count; k++) {
			int idx = find_row_index(tm, &keys[k]);
			if (idx >= 0 && n < OSP_MAX_ARRAY_LEN) {
				osp_ic_row_to_value(&tm->rows[idx], &tm->retrieve_buf[n++]);
			}
		}
	}
	result->tag = OSP_TAG_ARRAY;
	result->as.array.elements.items = tm->retrieve_buf;
	result->as.array.elements.count = n;
	result->as.array.elements.capacity = OSP_MAX_ARRAY_LEN;
	return OSP_OK;
}

static osp_err_t tm_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_table_manager_t *tm = (osp_ic_table_manager_t *)inst;
	osp_value_t entries[OSP_MAX_ARRAY_LEN];
	uint8_t count = 0;

	switch (method_id) {
		case 1:
		case 2:
			if (!param) {
				return OSP_ERR_INVALID;
			}
			if (osp_ic_spodus_parse_entries_list(param, entries, OSP_MAX_ARRAY_LEN, &count) != OSP_OK) {
				return OSP_ERR_INVALID;
			}
			if (method_id == 1) {
				return tm_add_update(tm, entries, count);
			}
			return tm_remove(tm, entries, count);
		case 3:
			if (!result) {
				return OSP_ERR_INVALID;
			}
			*result = osp_val_u8(tm->row_count);
			return OSP_OK;
		case 4:
			if (!param || !result) {
				return OSP_ERR_INVALID;
			}
			if (osp_ic_spodus_parse_entries_list(param, entries, OSP_MAX_ARRAY_LEN, &count) != OSP_OK) {
				return OSP_ERR_INVALID;
			}
			return tm_retrieve(tm, entries, count, result);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static const uint8_t tm_attrs[] = {1};

static osp_err_t tm_get_attr_fixed(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_table_manager_t *tm = (const osp_ic_table_manager_t *)inst;
	if (attr_id != 1 || !result) {
		return OSP_ERR_NOT_FOUND;
	}
	return osp_ic_get_logical_name(result, &tm->logical_name);
}

static osp_err_t tm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_table_manager_class(), inst, buf, tm_attrs, 1);
}

static osp_err_t tm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_table_manager_class(), inst, buf, tm_attrs, 1);
}

static const osp_ic_class_t ic_table_manager = {
    .name = "Table Manager",
    .class_id = 8200,
    .version = 0,
    .get_attr = tm_get_attr_fixed,
    .set_attr = NULL,
    .invoke = tm_invoke,
    .serialize = tm_serialize,
    .deserialize = tm_deserialize,
    .instance_size = sizeof(osp_ic_table_manager_t),
};

const osp_ic_class_t *osp_ic_table_manager_class(void) {
	return &ic_table_manager;
}

void osp_ic_table_manager_init(osp_ic_table_manager_t *i, osp_obis_t ln) {
	if (!i) {
		return;
	}
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}

void osp_ic_table_manager_set_key_index(osp_ic_table_manager_t *i, uint8_t key_index) {
	if (i) {
		i->key_index = key_index;
	}
}
