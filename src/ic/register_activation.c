#include "register_activation.h"
#include <string.h>

static osp_err_t ra_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id >= 1 && method_id <= 3)
		return OSP_OK;
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_ra = {
    .name = "Register Activation",
    .class_id = 6,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = ra_invoke,
    .instance_size = sizeof(osp_ic_register_activation_t)
};

const osp_ic_class_t *osp_ic_register_activation_class(void) {
	return &ic_ra;
}

void osp_ic_register_activation_init(osp_ic_register_activation_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
