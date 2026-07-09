#include "parameter_monitor.h"
#include <string.h>
static const osp_ic_class_t ic_parameter_monitor = {
    .name = "Parameter Monitor",
    .class_id = 65,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_parameter_monitor_t)
};

const osp_ic_class_t *osp_ic_parameter_monitor_class(void) {
	return &ic_parameter_monitor;
}

void osp_ic_parameter_monitor_init(osp_ic_parameter_monitor_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
