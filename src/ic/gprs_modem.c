#include "gprs_modem.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t gprs_attrs[] = {1, 2, 3, 4};

static osp_value_t gprs_qos_value(void) {
	static osp_value_t fields[2];
	osp_value_t v = {0};
	fields[0] = osp_val_null();
	fields[1] = osp_val_null();
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

static osp_err_t gprs_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_gprs_modem_t *g = (const osp_ic_gprs_modem_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &g->logical_name);
		case 2:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = g->apn_len;
			memcpy(result->as.octetstring.data, g->apn, g->apn_len);
			return OSP_OK;
		case 3:
			*result = osp_val_u16(g->pin_code);
			return OSP_OK;
		case 4:
			*result = gprs_qos_value();
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t gprs_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_gprs_modem_t *g = (osp_ic_gprs_modem_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len >= OSP_MAX_NAME_LEN) {
				return OSP_ERR_INVALID;
			}
			g->apn_len = (uint8_t)value->as.octetstring.len;
			memcpy(g->apn, value->as.octetstring.data, g->apn_len);
			return OSP_OK;
		case 3:
			g->pin_code = osp_get_u16(value);
			return OSP_OK;
		case 4:
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t gprs_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_gprs_modem_class(), inst, buf, gprs_attrs, 4);
}

static osp_err_t gprs_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_gprs_modem_class(), inst, buf, gprs_attrs, 4);
}

static const osp_ic_class_t ic_gprs_modem = {
    .name = "GPRS Modem Setup",
    .class_id = 45,
    .version = 0,
    .get_attr = gprs_get,
    .set_attr = gprs_set,
    .invoke = NULL,
    .serialize = gprs_serialize,
    .deserialize = gprs_deserialize,
    .instance_size = sizeof(osp_ic_gprs_modem_t),
};

const osp_ic_class_t *osp_ic_gprs_modem_class(void) {
	return &ic_gprs_modem;
}

void osp_ic_gprs_modem_init(osp_ic_gprs_modem_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
