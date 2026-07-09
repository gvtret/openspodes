#include "schedule.h"
#include <string.h>

static osp_err_t sched_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_schedule_t *s = (osp_ic_schedule_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		if (s->entry_count > 0)
			s->entry_count--;
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static const osp_ic_class_t ic_sched = {
    .name = "Schedule", .class_id = 10, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = sched_invoke, .instance_size = sizeof(osp_ic_schedule_t)
};

const osp_ic_class_t *osp_ic_schedule_class(void) {
	return &ic_sched;
}

void osp_ic_schedule_init(osp_ic_schedule_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
