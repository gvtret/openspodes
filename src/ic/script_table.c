#include "script_table.h"
#include <string.h>

static osp_err_t st_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();
	return (method_id == 1) ? OSP_OK : OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_st = {
    .name = "Script Table", .class_id = 9, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = st_invoke, .instance_size = sizeof(osp_ic_script_table_t)
};

const osp_ic_class_t *osp_ic_script_table_class(void) {
	return &ic_st;
}

void osp_ic_script_table_init(osp_ic_script_table_t *t, osp_obis_t ln) {
	memset(t, 0, sizeof(*t));
	t->logical_name = ln;
}
