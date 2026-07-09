#include "special_days.h"
#include <string.h>

static osp_err_t sd_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_special_days_t *d = (osp_ic_special_days_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		if (d->entry_count < 32)
			d->entry_count++;
		return OSP_OK;
	}
	if (method_id == 2) {
		if (d->entry_count > 0)
			d->entry_count--;
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_sd = {
    .name = "Special Days",
    .class_id = 11,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = sd_invoke,
    .instance_size = sizeof(osp_ic_special_days_t)
};

const osp_ic_class_t *osp_ic_special_days_class(void) {
	return &ic_sd;
}

void osp_ic_special_days_init(osp_ic_special_days_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
