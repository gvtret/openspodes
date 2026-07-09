#include "arbitrator.h"
#include <string.h>

static osp_err_t arb_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)param;
	*result = osp_val_null();
	return (method_id == 1 || method_id == 2) ? OSP_OK : OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_arb = {
    .name = "Arbitrator", .class_id = 68, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = arb_invoke, .instance_size = sizeof(osp_ic_arbitrator_t)
};

const osp_ic_class_t *osp_ic_arbitrator_class(void) {
	return &ic_arb;
}

void osp_ic_arbitrator_init(osp_ic_arbitrator_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
