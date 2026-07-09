#include "mac_address.h"
#include <string.h>
static const osp_ic_class_t ic_mac_address = {
    .name = "MAC Address Setup", .class_id = 43, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_mac_address_t)
};

const osp_ic_class_t *osp_ic_mac_address_class(void) {
	return &ic_mac_address;
}

void osp_ic_mac_address_init(osp_ic_mac_address_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
