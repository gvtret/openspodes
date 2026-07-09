#include "tcp_udp_setup.h"
#include <string.h>

static osp_err_t tcp_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_tcp_udp_setup_t *t = (const osp_ic_tcp_udp_setup_t *)inst;
	if (attr_id == 6) {
		*result = osp_val_u16(t->port);
		return OSP_OK;
	}
	if (attr_id == 7) {
		*result = osp_val_bool(t->tcp_no_delay);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static const osp_ic_class_t ic_tcp = {
    .name = "TCP-UDP Setup",
    .class_id = 41,
    .version = 0,
    .get_attr = tcp_get,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_tcp_udp_setup_t)
};

const osp_ic_class_t *osp_ic_tcp_udp_setup_class(void) {
	return &ic_tcp;
}

void osp_ic_tcp_udp_setup_init(osp_ic_tcp_udp_setup_t *t, osp_obis_t ln) {
	memset(t, 0, sizeof(*t));
	t->logical_name = ln;
}
