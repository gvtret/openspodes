#include "ipv6_setup.h"
#include <string.h>
static const osp_ic_class_t ic_ipv6_setup = {
    .name = "IPv6 Setup", .class_id = 48, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_ipv6_setup_t)
};

const osp_ic_class_t *osp_ic_ipv6_setup_class(void) {
	return &ic_ipv6_setup;
}

void osp_ic_ipv6_setup_init(osp_ic_ipv6_setup_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
