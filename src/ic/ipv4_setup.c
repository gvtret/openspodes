#include "ipv4_setup.h"
#include <string.h>

static osp_err_t ipv4_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_ipv4_setup_t *s = (const osp_ic_ipv4_setup_t *)inst;
	switch (attr_id) {
		case 3:
			*result = osp_val_u32(s->ip_address);
			return OSP_OK;
		case 6:
			*result = osp_val_u32(s->subnet_mask);
			return OSP_OK;
		case 7:
			*result = osp_val_u32(s->gateway_ip);
			return OSP_OK;
		case 8:
			*result = osp_val_bool(s->use_dhcp);
			return OSP_OK;
		case 9:
			*result = osp_val_u32(s->primary_dns);
			return OSP_OK;
		case 10:
			*result = osp_val_u32(s->secondary_dns);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static const osp_ic_class_t ic_ipv4 = {
    .name = "IPv4 Setup", .class_id = 42, .version = 0, .get_attr = ipv4_get, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_ipv4_setup_t)
};

const osp_ic_class_t *osp_ic_ipv4_setup_class(void) {
	return &ic_ipv4;
}

void osp_ic_ipv4_setup_init(osp_ic_ipv4_setup_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
