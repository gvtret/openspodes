#include "extended_register.h"
#include <string.h>

static osp_err_t er_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_ext_register_t *r = (const osp_ic_ext_register_t *)inst;
	if (attr_id == 2) {
		*result = r->value;
		return OSP_OK;
	}
	if (attr_id == 4) {
		*result = r->status;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t er_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_ext_register_t *r = (osp_ic_ext_register_t *)inst;
	if (attr_id == 2 && value) {
		r->value = *value;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t er_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_ext_register_t *r = (osp_ic_ext_register_t *)inst;
	(void)param;
	if (method_id == 1) {
		r->value = osp_val_i32(0);
		*result = osp_val_null();
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_ext_reg = {
    .name = "Extended Register",
    .class_id = 4,
    .version = 0,
    .get_attr = er_get,
    .set_attr = er_set,
    .invoke = er_invoke,
    .instance_size = sizeof(osp_ic_ext_register_t)
};

const osp_ic_class_t *osp_ic_ext_register_class(void) {
	return &ic_ext_reg;
}

void osp_ic_ext_register_init(osp_ic_ext_register_t *r, osp_obis_t ln) {
	memset(r, 0, sizeof(*r));
	r->logical_name = ln;
}
