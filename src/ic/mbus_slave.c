#include "mbus_slave.h"
#include <string.h>
static const osp_ic_class_t ic_mbus_slave = {
    .name = "MBus Slave Port Setup",
    .class_id = 76,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_mbus_slave_t)
};

const osp_ic_class_t *osp_ic_mbus_slave_class(void) {
	return &ic_mbus_slave;
}

void osp_ic_mbus_slave_init(osp_ic_mbus_slave_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
