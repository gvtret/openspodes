#include "iec_local_port_setup.h"
#include <string.h>
static const osp_ic_class_t ic_local = {
    .name = "IEC Local Port Setup",
    .class_id = 19,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_iec_local_port_setup_t)
};

const osp_ic_class_t *osp_ic_iec_local_port_setup_class(void) {
	return &ic_local;
}

void osp_ic_iec_local_port_setup_init(osp_ic_iec_local_port_setup_t *p, osp_obis_t ln) {
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}
