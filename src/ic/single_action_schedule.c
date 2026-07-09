#include "single_action_schedule.h"
#include <string.h>
static const osp_ic_class_t ic_sas = {
    .name = "Single Action Schedule",
    .class_id = 22,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_single_action_schedule_t)
};

const osp_ic_class_t *osp_ic_single_action_schedule_class(void) {
	return &ic_sas;
}

void osp_ic_single_action_schedule_init(osp_ic_single_action_schedule_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
