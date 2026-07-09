#include "activity_calendar.h"
#include <string.h>

static osp_err_t ac_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)param;
	*result = osp_val_null();
	if (method_id == 1)
		return OSP_OK;
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_ac = {
    .name = "Activity Calendar",
    .class_id = 20,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = ac_invoke,
    .instance_size = sizeof(osp_ic_activity_calendar_t)
};

const osp_ic_class_t *osp_ic_activity_calendar_class(void) {
	return &ic_ac;
}

void osp_ic_activity_calendar_init(osp_ic_activity_calendar_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
