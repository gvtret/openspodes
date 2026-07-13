#include "script_table.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include "../codec/codec.h"
#include <string.h>

static const uint8_t st_attrs[] = {1, 2};

/* ── Helper: find script by script_id ────────────────────────────────────── */

static osp_script_t *find_script(osp_ic_script_table_t *t, uint32_t script_id) {
	for (uint8_t i = 0; i < t->script_count; i++) {
		if (t->scripts[i].script_id == script_id) {
			return &t->scripts[i];
		}
	}
	return NULL;
}

/* ── get_attr ───────────────────────────────────────────────────────────── */

static osp_err_t st_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_script_table_t *t = (const osp_ic_script_table_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &t->logical_name);
		case 2:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── set_attr ───────────────────────────────────────────────────────────── */

static osp_err_t st_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	(void)inst;
	if (attr_id == 2) {
		if (value->tag != OSP_TAG_ARRAY) return OSP_ERR_INVALID;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

/* ── invoke (method 1: execute_script) ──────────────────────────────────── */

static osp_err_t st_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_script_table_t *t = (osp_ic_script_table_t *)inst;
	(void)result;

	if (method_id != 1) return OSP_ERR_UNSUPPORTED;
	if (!param) return OSP_ERR_INVALID;

	uint32_t script_id;
	if (param->tag == OSP_TAG_DOUBLE_LONG_UNS) {
		script_id = param->as.uint32.value;
	} else if (param->tag == OSP_TAG_UNSIGNED) {
		script_id = param->as.uint8.value;
	} else if (param->tag == OSP_TAG_LONG_UNSIGNED) {
		script_id = param->as.uint16.value;
	} else {
		return OSP_ERR_INVALID;
	}

	osp_script_t *script = find_script(t, script_id);
	if (!script) return OSP_ERR_NOT_FOUND;

	/* Return the first action's method_param as proof of execution */
	if (result && script->action_count > 0) {
		*result = script->actions[0].method_param;
	} else if (result) {
		*result = osp_val_null();
	}
	return OSP_OK;
}

/* ── serialize ──────────────────────────────────────────────────────────── */

static osp_err_t st_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_script_table_class(), inst, buf, st_attrs, 2);
}

/* ── deserialize ────────────────────────────────────────────────────────── */

static osp_err_t st_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_script_table_class(), inst, buf, st_attrs, 2);
}

/* ── class definition ───────────────────────────────────────────────────── */

static const osp_ic_class_t ic_st = {
    .name = "Script Table",
    .class_id = 9,
    .version = 0,
    .get_attr = st_get,
    .set_attr = st_set,
    .invoke = st_invoke,
    .serialize = st_serialize,
    .deserialize = st_deserialize,
    .instance_size = sizeof(osp_ic_script_table_t),
};

const osp_ic_class_t *osp_ic_script_table_class(void) {
	return &ic_st;
}

void osp_ic_script_table_init(osp_ic_script_table_t *t, osp_obis_t ln) {
	memset(t, 0, sizeof(*t));
	t->logical_name = ln;
}
