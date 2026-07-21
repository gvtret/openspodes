#include "push_setup.h"
#include "ic_common.h"
#include "../server/server.h"
#include "../service/notification.h"
#include "../service/push_delivery.h"
#include "../codec/serialize.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t push_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

static osp_value_t push_object_list_val(const osp_ic_push_setup_t *p) {
	OSP_TLS osp_value_t items[OSP_MAX_PUSH_OBJECTS];
	OSP_TLS osp_value_t fields[OSP_MAX_PUSH_OBJECTS][4];
	osp_value_t v = {0};
	uint8_t n = p->push_object_count;
	if (n > OSP_MAX_PUSH_OBJECTS) {
		n = OSP_MAX_PUSH_OBJECTS;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_push_object_t *po = &p->push_object_list[i];
		fields[i][0] = osp_val_u16(po->class_id);
		fields[i][1].tag = OSP_TAG_OCTETSTRING;
		fields[i][1].as.octetstring.len = 6;
		memcpy(fields[i][1].as.octetstring.data, &po->logical_name, 6);
		fields[i][2] = osp_val_i8(po->attribute_index);
		fields[i][3] = osp_val_u16(po->data_index);
		items[i].tag = OSP_TAG_STRUCTURE;
		items[i].as.structure.elements.items = fields[i];
		items[i].as.structure.elements.count = 4;
		items[i].as.structure.elements.capacity = 4;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = OSP_MAX_PUSH_OBJECTS;
	return v;
}

static osp_value_t push_destination_val(const osp_send_destination_t *d) {
	OSP_TLS osp_value_t fields[3];
	osp_value_t v = {0};
	fields[0] = osp_val_enum((uint8_t)(d ? d->transport_service : 0));
	fields[1].tag = OSP_TAG_OCTETSTRING;
	fields[1].as.octetstring.len = d ? (uint16_t)d->destination_len : 0;
	if (d && d->destination_len > 0) {
		uint32_t len = d->destination_len;
		if (len > OSP_MAX_OCTET_LEN) {
			len = OSP_MAX_OCTET_LEN;
		}
		fields[1].as.octetstring.len = (uint16_t)len;
		memcpy(fields[1].as.octetstring.data, d->destination, len);
	}
	fields[2] = osp_val_enum((uint8_t)(d ? d->message : 0));
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 3;
	v.as.structure.elements.capacity = 3;
	return v;
}

static osp_value_t push_comm_window_val(const osp_ic_push_setup_t *p) {
	OSP_TLS osp_value_t items[OSP_MAX_COMM_WINDOW];
	OSP_TLS osp_value_t fields[OSP_MAX_COMM_WINDOW][2];
	osp_value_t v = {0};
	uint8_t n = p->comm_window_count;
	if (n > OSP_MAX_COMM_WINDOW) {
		n = OSP_MAX_COMM_WINDOW;
	}
	for (uint8_t i = 0; i < n; i++) {
		fields[i][0].tag = OSP_TAG_OCTETSTRING;
		fields[i][0].as.octetstring.len = OSP_COSEM_DATETIME_LEN;
		memcpy(fields[i][0].as.octetstring.data, p->communication_window[i].start, OSP_COSEM_DATETIME_LEN);
		fields[i][1].tag = OSP_TAG_OCTETSTRING;
		fields[i][1].as.octetstring.len = OSP_COSEM_DATETIME_LEN;
		memcpy(fields[i][1].as.octetstring.data, p->communication_window[i].end, OSP_COSEM_DATETIME_LEN);
		items[i].tag = OSP_TAG_STRUCTURE;
		items[i].as.structure.elements.items = fields[i];
		items[i].as.structure.elements.count = 2;
		items[i].as.structure.elements.capacity = 2;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = OSP_MAX_COMM_WINDOW;
	return v;
}

static osp_value_t push_repetition_delay_val(const osp_repetition_delay_t *rd) {
	OSP_TLS osp_value_t fields[3];
	osp_value_t v = {0};
	fields[0] = osp_val_u16(rd ? rd->repetition_delay_min : 0);
	fields[1] = osp_val_u16(rd ? rd->repetition_delay_exponent : 0);
	fields[2] = osp_val_u16(rd ? rd->repetition_delay_max : 0);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 3;
	v.as.structure.elements.capacity = 3;
	return v;
}

static osp_err_t push_read_object_list(const osp_value_t *val, osp_ic_push_setup_t *p) {
	if (!val || !p || val->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = val->as.array.elements.count;
	if (n > OSP_MAX_PUSH_OBJECTS) {
		n = OSP_MAX_PUSH_OBJECTS;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_value_t *el = &val->as.array.elements.items[i];
		if (el->tag != OSP_TAG_STRUCTURE || el->as.structure.elements.count < 4) {
			return OSP_ERR_INVALID;
		}
		p->push_object_list[i].class_id = osp_get_u16(&el->as.structure.elements.items[0]);
		const osp_value_t *ln = &el->as.structure.elements.items[1];
		if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
			return OSP_ERR_INVALID;
		}
		memcpy(&p->push_object_list[i].logical_name, ln->as.octetstring.data, 6);
		p->push_object_list[i].attribute_index = osp_get_i8(&el->as.structure.elements.items[2]);
		p->push_object_list[i].data_index = osp_get_u16(&el->as.structure.elements.items[3]);
	}
	p->push_object_count = n;
	return OSP_OK;
}

static osp_err_t push_read_destination(const osp_value_t *val, osp_send_destination_t *d) {
	if (!val || !d || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 3) {
		return OSP_ERR_INVALID;
	}
	d->transport_service = (osp_push_transport_t)osp_get_enum(&val->as.structure.elements.items[0]);
	const osp_value_t *dest = &val->as.structure.elements.items[1];
	if (dest->tag != OSP_TAG_OCTETSTRING) {
		return OSP_ERR_INVALID;
	}
	uint32_t len = dest->as.octetstring.len;
	if (len > sizeof(d->destination)) {
		len = sizeof(d->destination);
	}
	memcpy(d->destination, dest->as.octetstring.data, len);
	d->destination_len = len;
	d->message = (osp_push_message_t)osp_get_enum(&val->as.structure.elements.items[2]);
	return OSP_OK;
}

static osp_err_t push_read_comm_window(const osp_value_t *val, osp_ic_push_setup_t *p) {
	if (!val || !p || val->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = val->as.array.elements.count;
	if (n > OSP_MAX_COMM_WINDOW) {
		n = OSP_MAX_COMM_WINDOW;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_value_t *el = &val->as.array.elements.items[i];
		if (el->tag != OSP_TAG_STRUCTURE || el->as.structure.elements.count < 2) {
			return OSP_ERR_INVALID;
		}
		const osp_value_t *st = &el->as.structure.elements.items[0];
		const osp_value_t *en = &el->as.structure.elements.items[1];
		if (st->tag != OSP_TAG_OCTETSTRING || st->as.octetstring.len != OSP_COSEM_DATETIME_LEN ||
		    en->tag != OSP_TAG_OCTETSTRING || en->as.octetstring.len != OSP_COSEM_DATETIME_LEN) {
			return OSP_ERR_INVALID;
		}
		memcpy(p->communication_window[i].start, st->as.octetstring.data, OSP_COSEM_DATETIME_LEN);
		memcpy(p->communication_window[i].end, en->as.octetstring.data, OSP_COSEM_DATETIME_LEN);
	}
	p->comm_window_count = n;
	return OSP_OK;
}

static osp_err_t push_read_repetition_delay(const osp_value_t *val, osp_repetition_delay_t *rd) {
	if (!val || !rd || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 3) {
		return OSP_ERR_INVALID;
	}
	rd->repetition_delay_min = osp_get_u16(&val->as.structure.elements.items[0]);
	rd->repetition_delay_exponent = osp_get_u16(&val->as.structure.elements.items[1]);
	rd->repetition_delay_max = osp_get_u16(&val->as.structure.elements.items[2]);
	return OSP_OK;
}

static osp_value_t push_confirmation_params_val(const osp_confirmation_parameters_t *cp) {
	OSP_TLS osp_value_t fields[2];
	osp_value_t v = {0};
	if (cp) {
		fields[0].tag = OSP_TAG_DATETIME;
		fields[0].as.datetime = cp->confirmation_start_date;
		fields[1] = osp_val_u32(cp->confirmation_interval);
	} else {
		fields[0] = osp_val_datetime(1900, 1, 1, 7, 0, 0, 0, 0);
		fields[1] = osp_val_u32(0);
	}
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

static osp_err_t push_read_datetime(const osp_value_t *val, osp_datetime_t *dt) {
	if (!val || !dt) {
		return OSP_ERR_INVALID;
	}
	if (val->tag == OSP_TAG_DATETIME) {
		*dt = val->as.datetime;
		return OSP_OK;
	}
	if (val->tag == OSP_TAG_OCTETSTRING && val->as.octetstring.len == OSP_COSEM_DATETIME_LEN) {
		osp_cosem_datetime_t cd;
		osp_cosem_datetime_from_bytes(&cd, val->as.octetstring.data);
		dt->date.year = cd.year;
		dt->date.month = cd.month;
		dt->date.day = cd.day;
		dt->date.day_of_week = cd.day_of_week;
		dt->time.hour = cd.hour;
		dt->time.minute = cd.minute;
		dt->time.second = cd.second;
		dt->time.ms = cd.hundredths;
		return OSP_OK;
	}
	return OSP_ERR_INVALID;
}

static osp_err_t push_read_confirmation_params(const osp_value_t *val, osp_confirmation_parameters_t *cp) {
	if (!val || !cp || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	osp_err_t r = push_read_datetime(&val->as.structure.elements.items[0], &cp->confirmation_start_date);
	if (r != OSP_OK) {
		return r;
	}
	cp->confirmation_interval = osp_get_u32(&val->as.structure.elements.items[1]);
	return OSP_OK;
}

static osp_err_t push_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_push_setup_t *p = (const osp_ic_push_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &p->logical_name);
		case 2:
			*result = push_object_list_val(p);
			return OSP_OK;
		case 3:
			*result = push_destination_val(&p->send_destination);
			return OSP_OK;
		case 4:
			*result = push_comm_window_val(p);
			return OSP_OK;
		case 5:
			*result = osp_val_u16(p->randomisation_start_interval);
			return OSP_OK;
		case 6:
			*result = osp_val_u8(p->number_of_retries);
			return OSP_OK;
		case 7:
			*result = push_repetition_delay_val(&p->repetition_delay);
			return OSP_OK;
		case 8:
			return osp_ic_get_logical_name(result, &p->port_reference);
		case 9:
			*result = osp_val_i8(p->push_client_SAP);
			return OSP_OK;
		case 10:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 11:
			*result = osp_val_enum(p->push_operation_method);
			return OSP_OK;
		case 12:
			*result = push_confirmation_params_val(&p->confirmation_parameters);
			return OSP_OK;
		case 13: {
			osp_value_t v = {0};
			v.tag = OSP_TAG_DATETIME;
			v.as.datetime = p->last_confirmation_date_time;
			*result = v;
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t push_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_push_setup_t *p = (osp_ic_push_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return push_read_object_list(value, p);
		case 3:
			return push_read_destination(value, &p->send_destination);
		case 4:
			return push_read_comm_window(value, p);
		case 5:
			p->randomisation_start_interval = osp_get_u16(value);
			return OSP_OK;
		case 6:
			p->number_of_retries = osp_get_u8(value);
			return OSP_OK;
		case 7:
			return push_read_repetition_delay(value, &p->repetition_delay);
		case 8:
			return osp_ic_set_logical_name(&p->port_reference, value);
		case 9:
			p->push_client_SAP = osp_get_i8(value);
			return OSP_OK;
		case 10:
			return OSP_OK; /* empty / ignore writes for now */
		case 11:
			p->push_operation_method = osp_get_enum(value);
			return OSP_OK;
		case 12:
			return push_read_confirmation_params(value, &p->confirmation_parameters);
		case 13:
			return push_read_datetime(value, &p->last_confirmation_date_time);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t push_build_notification_body(osp_dispatcher_t *disp, const osp_ic_push_setup_t *p, osp_value_t *out) {
	if (!disp || !p || !out || p->push_object_count == 0) {
		return OSP_ERR_INVALID;
	}

	const osp_push_object_t *po0 = &p->push_object_list[0];
	if (po0->class_id == 62 && po0->attribute_index == 2) {
		osp_value_t ignored = osp_val_null();
		osp_err_t r = osp_dispatcher_action(disp, po0->class_id, &po0->logical_name, 2, NULL, &ignored);
		if (r != OSP_OK) {
			return r;
		}
		return osp_dispatcher_get(disp, po0->class_id, &po0->logical_name, 2, out);
	}

	OSP_TLS osp_value_t items[OSP_MAX_PUSH_OBJECTS];
	uint8_t n = p->push_object_count;
	if (n > OSP_MAX_PUSH_OBJECTS) {
		n = OSP_MAX_PUSH_OBJECTS;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_push_object_t *po = &p->push_object_list[i];
		osp_err_t r = osp_dispatcher_get(disp, po->class_id, &po->logical_name, (uint8_t)po->attribute_index, &items[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	out->tag = OSP_TAG_ARRAY;
	out->as.array.elements.items = items;
	out->as.array.elements.count = n;
	out->as.array.elements.capacity = n;
	return OSP_OK;
}

static osp_err_t push_try_schedule_delivery(const osp_ic_push_setup_t *p, const osp_value_t *body) {
	if (!p || !body) {
		return OSP_ERR_INVALID;
	}
	if (p->send_destination.message != OSP_PUSH_MSG_DATA_NOTIFICATION) {
		return OSP_ERR_UNSUPPORTED;
	}

	osp_push_delivery_request_t req;
	memset(&req, 0, sizeof(req));
	req.transport_service = (uint8_t)p->send_destination.transport_service;
	req.client_sap = p->push_client_SAP;
	if (p->send_destination.destination_len > 0) {
		uint32_t len = p->send_destination.destination_len;
		if (len > OSP_PUSH_DEST_MAX) {
			len = OSP_PUSH_DEST_MAX;
		}
		memcpy(req.destination, p->send_destination.destination, len);
		req.destination_len = len;
	}

	osp_buf_t wb;
	osp_buf_init(&wb, req.body, sizeof(req.body));
	if (osp_value_write(&wb, body) != OSP_OK) {
		return OSP_ERR_INVALID;
	}
	req.body_len = wb.wr;
	return osp_push_schedule_delivery(&req);
}

static osp_err_t push_do_invoke(osp_ic_push_setup_t *p, const osp_value_t *param, osp_value_t *result) {
	(void)param;
	if (result) {
		*result = osp_val_null();
	}
	if (!p->server || p->push_object_count == 0) {
		return OSP_ERR_INVALID;
	}

	osp_value_t body;
	osp_err_t r = push_build_notification_body(&p->server->dispatcher, p, &body);
	if (r != OSP_OK) {
		return r;
	}

	const bool confirmed = p->push_operation_method == 2;

	/* multispodes: active HDLC link → in-band Data-Notification (deferred past ACTION). */
	if (p->server->hdlc_active) {
		return osp_server_queue_pending_push(p->server, &body, confirmed);
	}

	/* multispodes PreAction: outbound TCP/UDP before ACTION response (synchronous). */
	r = push_try_schedule_delivery(p, &body);
	if (r == OSP_OK) {
		return OSP_OK;
	}
	if (r != OSP_ERR_NOT_FOUND) {
		return r;
	}

	/* Fallback: inline DataNotification on the current association (unit tests). */
	return osp_server_queue_pending_push(p->server, &body, confirmed);
}

static osp_err_t push_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_push_setup_t *p = (osp_ic_push_setup_t *)inst;
	if (osp_hal_data && osp_hal_data->execute) {
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, &p->logical_name, method_id, param, result);
		if (r == OSP_OK) {
			return OSP_OK;
		}
		if (r != OSP_ERR_NOT_FOUND) {
			return r;
		}
	}

	if (result) {
		*result = osp_val_null();
	}

	/* Push Setup v2 (СТО 7.3.16): 1 = push (data), 2 = reset */
	if (method_id == 1) {
		return push_do_invoke(p, param, result);
	}
	if (method_id == 2) {
		if (p->server) {
			p->server->pending_push.pending = false;
			p->server->pending_push.defer_flush = false;
		}
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t push_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_push_setup_class(), inst, buf, push_attrs, 13);
}

static osp_err_t push_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_push_setup_class(), inst, buf, push_attrs, 13);
}

static const osp_ic_class_t ic_push = {
    .name = "Push Setup",
    .class_id = 40,
    .version = 2,
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
	p->number_of_retries = 1;
	p->repetition_delay.repetition_delay_min = 5;
	p->repetition_delay.repetition_delay_exponent = 200;
	p->repetition_delay.repetition_delay_max = 3600;
	p->push_client_SAP = 0x40; /* инициативный клиент */
	p->push_operation_method = 2; /* confirmed, retry on missing confirmation */
	/* confirmation / last_confirmation: 1900-01-01 (etalon default) */
	p->confirmation_parameters.confirmation_start_date = (osp_datetime_t){
	    .date = {.year = 1900, .month = 1, .day = 1, .day_of_week = 7},
	    .time = {0},
	};
	p->last_confirmation_date_time = p->confirmation_parameters.confirmation_start_date;
}

void osp_ic_push_setup_bind_server(osp_ic_push_setup_t *p, osp_server_t *server) {
	if (p) {
		p->server = server;
	}
}
