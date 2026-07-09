#include "push_setup.h"
#include <string.h>

static osp_err_t push_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_push_setup_t *p = (const osp_ic_push_setup_t *)inst;
	switch (attr_id) {
		case 5:
			*result = osp_val_u16(p->randomisation_start_interval);
			return OSP_OK;
		case 6:
			*result = osp_val_u8(p->number_of_retries);
			return OSP_OK;
		case 7:
			*result = osp_val_u16(p->repetition_delay);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t push_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1)
		return OSP_OK;
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_push = {
    .name = "Push Setup",
    .class_id = 40,
    .version = 0,
    .get_attr = push_get,
    .set_attr = NULL,
    .invoke = push_invoke,
    .instance_size = sizeof(osp_ic_push_setup_t)
};

const osp_ic_class_t *osp_ic_push_setup_class(void) {
	return &ic_push;
}

void osp_ic_push_setup_init(osp_ic_push_setup_t *p, osp_obis_t ln) {
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}
