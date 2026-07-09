#include "demand_register.h"
#include <string.h>

static osp_err_t dr_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_demand_register_t *d = (osp_ic_demand_register_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		d->current_average_value = osp_val_i32(0);
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_dr = {
    .name = "Demand Register",
    .class_id = 5,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = dr_invoke,
    .instance_size = sizeof(osp_ic_demand_register_t)
};

const osp_ic_class_t *osp_ic_demand_register_class(void) {
	return &ic_dr;
}

void osp_ic_demand_register_init(osp_ic_demand_register_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
