#include "gsm_diagnostic.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t gsm_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8};

static osp_err_t gsm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_gsm_diagnostic_t *g = (const osp_ic_gsm_diagnostic_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &g->logical_name);
		case 2:
			result->tag = OSP_TAG_VISIBLESTRING;
			result->as.visiblestring.len = g->operator_len;
			memcpy(result->as.visiblestring.data, g->operator_name, g->operator_len);
			result->as.visiblestring.data[g->operator_len] = '\0';
			return OSP_OK;
		case 3:
			*result = osp_val_enum(g->status);
			return OSP_OK;
		case 4:
			*result = osp_val_enum(g->cs_attachment);
			return OSP_OK;
		case 5:
			*result = osp_val_enum(g->ps_status);
			return OSP_OK;
		case 6:
			*result = osp_val_null();
			return OSP_OK;
		case 7:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 8:
			result->tag = OSP_TAG_DATETIME;
			result->as.datetime = g->capture_time;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t gsm_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_gsm_diagnostic_t *g = (osp_ic_gsm_diagnostic_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			if (value->tag != OSP_TAG_VISIBLESTRING || value->as.visiblestring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			g->operator_len = (uint8_t)value->as.visiblestring.len;
			memcpy(g->operator_name, value->as.visiblestring.data, g->operator_len);
			g->operator_name[g->operator_len] = '\0';
			return OSP_OK;
		case 3:
			g->status = osp_get_enum(value);
			return OSP_OK;
		case 4:
			g->cs_attachment = osp_get_enum(value);
			return OSP_OK;
		case 5:
			g->ps_status = osp_get_enum(value);
			return OSP_OK;
		case 6:
		case 7:
			return OSP_OK;
		case 8:
			if (value->tag != OSP_TAG_DATETIME) {
				return OSP_ERR_INVALID;
			}
			g->capture_time = value->as.datetime;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t gsm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_gsm_diagnostic_class(), inst, buf, gsm_attrs, 8);
}

static osp_err_t gsm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_gsm_diagnostic_class(), inst, buf, gsm_attrs, 8);
}

static const osp_ic_class_t ic_gsm_diagnostic = {
    .name = "GSM Diagnostic",
    .class_id = 47,
    .version = 0,
    .get_attr = gsm_get,
    .set_attr = gsm_set,
    .invoke = NULL,
    .serialize = gsm_serialize,
    .deserialize = gsm_deserialize,
    .instance_size = sizeof(osp_ic_gsm_diagnostic_t),
};

const osp_ic_class_t *osp_ic_gsm_diagnostic_class(void) {
	return &ic_gsm_diagnostic;
}

void osp_ic_gsm_diagnostic_init(osp_ic_gsm_diagnostic_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
