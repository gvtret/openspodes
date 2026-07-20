#include "security_setup.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ss_attrs[] = {1, 2, 3, 4, 5};

static osp_err_t ss_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_security_setup_t *s = (const osp_ic_security_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			*result = osp_val_u8(s->security_policy);
			return OSP_OK;
		case 3:
			*result = osp_val_u8(s->security_suite);
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = s->client_system_title.len;
			memcpy(result->as.octetstring.data, s->client_system_title.data, s->client_system_title.len);
			return OSP_OK;
		case 5:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = s->server_system_title.len;
			memcpy(result->as.octetstring.data, s->server_system_title.data, s->server_system_title.len);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ss_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_security_setup_t *s = (osp_ic_security_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			s->security_policy = osp_get_u8(value);
			return OSP_OK;
		case 3:
			s->security_suite = osp_get_u8(value);
			return OSP_OK;
		case 4:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			s->client_system_title.len = value->as.octetstring.len;
			memcpy(s->client_system_title.data, value->as.octetstring.data, value->as.octetstring.len);
			return OSP_OK;
		case 5:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			s->server_system_title.len = value->as.octetstring.len;
			memcpy(s->server_system_title.data, value->as.octetstring.data, value->as.octetstring.len);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ss_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	(void)inst;
	(void)param;
	if (method_id == 1 || method_id == 2) {
		*result = osp_val_null();
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t ss_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_security_setup_class(), inst, buf, ss_attrs, 5);
}

static osp_err_t ss_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_security_setup_class(), inst, buf, ss_attrs, 5);
}

static const osp_ic_class_t ic_ss = {
    .name = "Security Setup",
    .class_id = 64,
    .version = 0,
    .get_attr = ss_get,
    .set_attr = ss_set,
    .invoke = ss_invoke,
    .serialize = ss_serialize,
    .deserialize = ss_deserialize,
    .instance_size = sizeof(osp_ic_security_setup_t),
};

const osp_ic_class_t *osp_ic_security_setup_class(void) {
	return &ic_ss;
}

void osp_ic_security_setup_init(osp_ic_security_setup_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
