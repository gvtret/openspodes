#include "security_setup.h"
#include <string.h>

static osp_err_t ss_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_security_setup_t *s = (const osp_ic_security_setup_t *)inst;
	switch (attr_id) {
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
	osp_ic_security_setup_t *s = (osp_ic_security_setup_t *)inst;
	if (attr_id == 4 && value && value->tag == OSP_TAG_OCTETSTRING) {
		s->client_system_title.len = value->as.octetstring.len;
		memcpy(s->client_system_title.data, value->as.octetstring.data, value->as.octetstring.len);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t ss_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	if (method_id == 1 || method_id == 2) {
		*result = osp_val_null();
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_ss = {
    .name = "Security Setup",
    .class_id = 64,
    .version = 0,
    .get_attr = ss_get,
    .set_attr = ss_set,
    .invoke = ss_invoke,
    .instance_size = sizeof(osp_ic_security_setup_t)
};

const osp_ic_class_t *osp_ic_security_setup_class(void) {
	return &ic_ss;
}

void osp_ic_security_setup_init(osp_ic_security_setup_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
