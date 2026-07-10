#include "tcp_udp_setup.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t tcp_attrs[] = {1, 6, 7};

static osp_err_t tcp_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_tcp_udp_setup_t *t = (const osp_ic_tcp_udp_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &t->logical_name);
		case 6:
			*result = osp_val_u16(t->port);
			return OSP_OK;
		case 7:
			*result = osp_val_bool(t->tcp_no_delay);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t tcp_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_tcp_udp_setup_t *t = (osp_ic_tcp_udp_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 6:
			t->port = osp_get_u16(value);
			return OSP_OK;
		case 7:
			t->tcp_no_delay = osp_get_bool(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t tcp_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_tcp_udp_setup_class(), inst, buf, tcp_attrs, 3);
}

static osp_err_t tcp_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_tcp_udp_setup_class(), inst, buf, tcp_attrs, 3);
}

static const osp_ic_class_t ic_tcp = {
    .name = "TCP-UDP Setup",
    .class_id = 41,
    .version = 0,
    .get_attr = tcp_get,
    .set_attr = tcp_set,
    .invoke = NULL,
    .serialize = tcp_serialize,
    .deserialize = tcp_deserialize,
    .instance_size = sizeof(osp_ic_tcp_udp_setup_t),
};

const osp_ic_class_t *osp_ic_tcp_udp_setup_class(void) {
	return &ic_tcp;
}

void osp_ic_tcp_udp_setup_init(osp_ic_tcp_udp_setup_t *t, osp_obis_t ln) {
	memset(t, 0, sizeof(*t));
	t->logical_name = ln;
}
