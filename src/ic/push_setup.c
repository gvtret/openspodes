#include "push_setup.h"
#include "ic_common.h"
#include "../server/server.h"
#include "../service/notification.h"
#include <string.h>

static const uint8_t push_attrs[] = {1, 5, 6, 7};

static osp_err_t push_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_push_setup_t *p = (const osp_ic_push_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &p->logical_name);
		case 5:
			*result = osp_val_u16(p->randomisation_start_interval);
			return OSP_OK;
		case 6:
			*result = osp_val_u8(p->number_of_retries);
			return OSP_OK;
		case 7:
			*result = osp_val_u16(p->repetition_delay);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t push_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_push_setup_t *p = (osp_ic_push_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 5:
			p->randomisation_start_interval = osp_get_u16(value);
			return OSP_OK;
		case 6:
			p->number_of_retries = osp_get_u8(value);
			return OSP_OK;
		case 7:
			p->repetition_delay = osp_get_u16(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t push_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_push_setup_t *p = (osp_ic_push_setup_t *)inst;
	(void)param;
	if (result) {
		*result = osp_val_null();
	}
	if (method_id != 1) {
		return OSP_ERR_UNSUPPORTED;
	}
	if (!p->server || p->push_object_count == 0) {
		return OSP_ERR_INVALID;
	}

	osp_server_t *server = p->server;
	const osp_push_object_t *po = &p->push_object_list[0];

	if (po->class_id == 62 && po->attribute_index == 2) {
		osp_value_t ignored = osp_val_null();
		osp_err_t r = osp_dispatcher_action(&server->dispatcher, po->class_id, &po->logical_name, 2, NULL, &ignored);
		if (r != OSP_OK) {
			return r;
		}
	}

	osp_value_t body;
	osp_err_t r = osp_dispatcher_get(&server->dispatcher, po->class_id, &po->logical_name, (uint8_t)po->attribute_index, &body);
	if (r != OSP_OK) {
		return r;
	}

	server->pending_push.pending = true;
	server->pending_push.notification.long_invoke_id_and_priority = 1;
	server->pending_push.notification.date_time_len = 0;
	server->pending_push.notification.notification_body = body;
	return OSP_OK;
}

static osp_err_t push_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_push_setup_class(), inst, buf, push_attrs, 4);
}

static osp_err_t push_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_push_setup_class(), inst, buf, push_attrs, 4);
}

static const osp_ic_class_t ic_push = {
    .name = "Push Setup",
    .class_id = 40,
    .version = 0,
    .get_attr = push_get,
    .set_attr = push_set,
    .invoke = push_invoke,
    .serialize = push_serialize,
    .deserialize = push_deserialize,
    .instance_size = sizeof(osp_ic_push_setup_t),
};

const osp_ic_class_t *osp_ic_push_setup_class(void) {
	return &ic_push;
}

void osp_ic_push_setup_init(osp_ic_push_setup_t *p, osp_obis_t ln) {
	if (!p) {
		return;
	}
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}

void osp_ic_push_setup_bind_server(osp_ic_push_setup_t *p, osp_server_t *server) {
	if (p) {
		p->server = server;
	}
}
