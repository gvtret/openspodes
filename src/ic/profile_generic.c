#include "profile_generic.h"
#include <string.h>

static osp_err_t pg_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_profile_generic_t *p = (const osp_ic_profile_generic_t *)inst;
	switch (attr_id) {
		case 4:
			*result = osp_val_u32(p->capture_period);
			return OSP_OK;
		case 5:
			*result = osp_val_u8((uint8_t)p->sort_method);
			return OSP_OK;
		case 7:
			*result = osp_val_u32(p->entries_in_use);
			return OSP_OK;
		case 8:
			*result = osp_val_u32(p->profile_entries);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pg_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_profile_generic_t *p = (osp_ic_profile_generic_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		p->buffer.row_count = 0;
		p->entries_in_use = 0;
		return OSP_OK;
	}
	if (method_id == 2) {
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_pg = {
    .name = "Profile Generic",
    .class_id = 7,
    .version = 1,
    .get_attr = pg_get,
    .set_attr = NULL,
    .invoke = pg_invoke,
    .instance_size = sizeof(osp_ic_profile_generic_t)
};

const osp_ic_class_t *osp_ic_profile_generic_class(void) {
	return &ic_pg;
}

void osp_ic_profile_generic_init(osp_ic_profile_generic_t *p, osp_obis_t ln) {
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}
