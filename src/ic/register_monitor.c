#include "register_monitor.h"
#include <string.h>
static const osp_ic_class_t ic_rm = {
    .name = "Register Monitor",
    .class_id = 21,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_register_monitor_t)
};

const osp_ic_class_t *osp_ic_register_monitor_class(void) {
	return &ic_rm;
}

void osp_ic_register_monitor_init(osp_ic_register_monitor_t *m, osp_obis_t ln) {
	memset(m, 0, sizeof(*m));
	m->logical_name = ln;
}
