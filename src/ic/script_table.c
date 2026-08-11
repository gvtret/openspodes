#include "script_table.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include "../codec/codec.h"
#include <string.h>
#include "../data_hal.h"

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

static osp_value_t st_scripts_val(const osp_ic_script_table_t *t) {
	OSP_TLS osp_script_list_view_t view;
	osp_value_t v = {0};
	uint8_t n = t->script_count;
	if (n > OSP_MAX_SCRIPTS) {
		n = OSP_MAX_SCRIPTS;
	}
	if (n == 0) {
		return osp_ic_val_empty_array();
	}
	view.scripts = t->scripts;
	view.count = n;
	v.tag = OSP_TAG_SCRIPT_LIST_REF;
	v.as.ref = &view;
	return v;
}

/* ── get_attr ───────────────────────────────────────────────────────────── */

static osp_err_t st_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_script_table_t *t = (const osp_ic_script_table_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &t->logical_name);
		case 2:
			*result = st_scripts_val(t);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

/* ── set_attr ───────────────────────────────────────────────────────────── */

static osp_err_t st_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	if (attr_id == 2) {
		if (!value || value->tag != OSP_TAG_ARRAY) {
			return OSP_ERR_INVALID;
		}
		osp_ic_script_table_t *t = (osp_ic_script_table_t *)inst;
		uint8_t n = value->as.array.elements.count;
		if (n > OSP_MAX_SCRIPTS) {
			n = OSP_MAX_SCRIPTS;
		}
		for (uint8_t i = 0; i < n; i++) {
			const osp_value_t *row = &value->as.array.elements.items[i];
			if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
				return OSP_ERR_INVALID;
			}
			osp_script_t *sc = &t->scripts[i];
			memset(sc, 0, sizeof(*sc));
			sc->script_id = osp_get_u32(&row->as.structure.elements.items[0]);
			const osp_value_t *acts = &row->as.structure.elements.items[1];
			if (acts->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			uint8_t an = acts->as.array.elements.count;
			if (an > OSP_MAX_SCRIPT_ACTIONS) {
				an = OSP_MAX_SCRIPT_ACTIONS;
			}
			sc->action_count = an;
			for (uint8_t j = 0; j < an; j++) {
				const osp_value_t *ai = &acts->as.array.elements.items[j];
				if (ai->tag != OSP_TAG_STRUCTURE || ai->as.structure.elements.count < 4) {
					return OSP_ERR_INVALID;
				}
				const osp_value_t *f = ai->as.structure.elements.items;
				sc->actions[j].class_id = osp_get_u16(&f[0]);
				if (f[1].tag == OSP_TAG_OCTETSTRING && f[1].as.octetstring.len >= 6) {
					memcpy(&sc->actions[j].logical_name, f[1].as.octetstring.data, 6);
				}
				sc->actions[j].method_id = osp_get_i8(&f[2]);
				sc->actions[j].method_param = f[3];
			}
		}
		t->script_count = n;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

/* ── invoke (method 1: execute_script) ──────────────────────────────────── */

static osp_err_t st_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

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
