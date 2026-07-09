#include "data_protection.h"
#include <string.h>
static const osp_ic_class_t ic_dp = {
    .name = "Data Protection",
    .class_id = 30,
    .version = 0,
    .get_attr = NULL,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_data_protection_t)
};

const osp_ic_class_t *osp_ic_data_protection_class(void) {
	return &ic_dp;
}

void osp_ic_data_protection_init(osp_ic_data_protection_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
