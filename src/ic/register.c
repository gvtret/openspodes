#include "register.h"
#include <string.h>

static osp_err_t reg_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_register_t *r = (const osp_ic_register_t *)inst;
	if (attr_id == 2) {
		*result = r->value;
		return OSP_OK;
	}
	if (attr_id == 3) {
		result->tag = OSP_TAG_STRUCTURE;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t reg_set_attr(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_register_t *r = (osp_ic_register_t *)inst;
	if (attr_id == 2 && value) {
		r->value = *value;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t reg_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_register_t *r = (osp_ic_register_t *)inst;
	(void)param;
	if (method_id == 1) {
		r->value = osp_val_i32(0);
		*result = osp_val_null();
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_register = {
    .name = "Register",
    .class_id = 3,
    .version = 0,
    .get_attr = reg_get_attr,
    .set_attr = reg_set_attr,
    .invoke = reg_invoke,
    .instance_size = sizeof(osp_ic_register_t)
};

const osp_ic_class_t *osp_ic_register_class(void) {
	return &ic_register;
}

void osp_ic_register_init(osp_ic_register_t *r, osp_obis_t ln, osp_value_t val) {
	memset(r, 0, sizeof(*r));
	r->logical_name = ln;
	r->value = val;
}
