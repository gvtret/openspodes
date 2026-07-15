/**
 * ic_common.c — Shared helpers for COSEM interface class implementations
 */

#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>

osp_err_t osp_ic_get_logical_name(osp_value_t *result, const osp_obis_t *ln) {
	if (!result || !ln) {
		return OSP_ERR_INVALID;
	}
	result->tag = OSP_TAG_OCTETSTRING;
	result->as.octetstring.len = 6;
	memcpy(result->as.octetstring.data, ln, 6);
	return OSP_OK;
}

osp_err_t osp_ic_set_logical_name(osp_obis_t *ln, const osp_value_t *value) {
	if (!ln || !value || value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 6) {
		return OSP_ERR_INVALID;
	}
	memcpy(ln, value->as.octetstring.data, 6);
	return OSP_OK;
}

osp_value_t osp_ic_val_scaler_unit(const osp_scaler_unit_t *su) {
	OSP_TLS osp_value_t fields[2];
	osp_value_t v = {0};
	fields[0] = osp_val_i8(su ? su->scaler : 0);
	fields[1] = osp_val_enum(su ? su->unit : 0);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

osp_err_t osp_ic_read_scaler_unit(const osp_value_t *val, osp_scaler_unit_t *su) {
	if (!val || !su || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	su->scaler = osp_get_i8(&val->as.structure.elements.items[0]);
	su->unit = osp_get_enum(&val->as.structure.elements.items[1]);
	return OSP_OK;
}

osp_value_t osp_ic_val_value_definition(const osp_value_definition_t *vd) {
	OSP_TLS osp_value_t fields[3];
	osp_value_t v = {0};
	fields[0] = osp_val_u16(vd ? vd->class_id : 0);
	fields[1].tag = OSP_TAG_OCTETSTRING;
	if (vd) {
		fields[1].as.octetstring.len = 6;
		memcpy(fields[1].as.octetstring.data, &vd->logical_name, 6);
	}
	fields[2] = osp_val_i8(vd ? vd->attribute_index : 0);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 3;
	v.as.structure.elements.capacity = 3;
	return v;
}

osp_err_t osp_ic_read_value_definition(const osp_value_t *val, osp_value_definition_t *vd) {
	if (!val || !vd || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 3) {
		return OSP_ERR_INVALID;
	}
	vd->class_id = osp_get_u16(&val->as.structure.elements.items[0]);
	const osp_value_t *ln = &val->as.structure.elements.items[1];
	if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
		return OSP_ERR_INVALID;
	}
	memcpy(&vd->logical_name, ln->as.octetstring.data, 6);
	vd->attribute_index = osp_get_i8(&val->as.structure.elements.items[2]);
	return OSP_OK;
}

osp_value_t osp_ic_val_empty_array(void) {
	OSP_TLS osp_value_t items[1];
	osp_value_t v = {0};
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = 0;
	v.as.array.elements.capacity = 0;
	return v;
}

osp_value_t osp_ic_val_xdms_context(const osp_xdms_context_t *ctx) {
	OSP_TLS osp_value_t fields[6];
	osp_value_t v = {0};
	fields[0] = osp_val_u32(ctx ? ctx->conformance : 0);
	fields[1] = osp_val_u16(ctx ? ctx->max_receive_pdu_size : 0);
	fields[2] = osp_val_u16(ctx ? ctx->max_send_pdu_size : 0);
	fields[3] = osp_val_u8(ctx ? ctx->dlms_version_number : 6);
	fields[4] = osp_val_i8(ctx ? ctx->quality_of_service : 0);
	fields[5].tag = OSP_TAG_OCTETSTRING;
	if (ctx && ctx->cyphering_info_len > 0) {
		uint32_t len = ctx->cyphering_info_len;
		if (len > OSP_MAX_OCTET_LEN) {
			len = OSP_MAX_OCTET_LEN;
		}
		fields[5].as.octetstring.len = len;
		memcpy(fields[5].as.octetstring.data, ctx->cyphering_info, len);
	}
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 6;
	v.as.structure.elements.capacity = 6;
	return v;
}

osp_err_t osp_ic_read_xdms_context(const osp_value_t *val, osp_xdms_context_t *ctx) {
	if (!val || !ctx || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 5) {
		return OSP_ERR_INVALID;
	}
	ctx->conformance = osp_get_u32(&val->as.structure.elements.items[0]);
	ctx->max_receive_pdu_size = osp_get_u16(&val->as.structure.elements.items[1]);
	ctx->max_send_pdu_size = osp_get_u16(&val->as.structure.elements.items[2]);
	ctx->dlms_version_number = osp_get_u8(&val->as.structure.elements.items[3]);
	ctx->quality_of_service = osp_get_i8(&val->as.structure.elements.items[4]);
	if (val->as.structure.elements.count > 5 && val->as.structure.elements.items[5].tag == OSP_TAG_OCTETSTRING) {
		const osp_value_t *ci = &val->as.structure.elements.items[5];
		ctx->cyphering_info_len = ci->as.octetstring.len;
		if (ctx->cyphering_info_len > sizeof(ctx->cyphering_info)) {
			ctx->cyphering_info_len = sizeof(ctx->cyphering_info);
		}
		memcpy(ctx->cyphering_info, ci->as.octetstring.data, ctx->cyphering_info_len);
	} else {
		ctx->cyphering_info_len = 0;
	}
	return OSP_OK;
}

osp_value_t osp_ic_val_associated_partners(const osp_associated_partners_t *p) {
	OSP_TLS osp_value_t fields[2];
	osp_value_t v = {0};
	fields[0] = osp_val_i8(p ? p->client_sap : 0);
	fields[1] = osp_val_u16(p ? p->server_sap : 0);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

osp_err_t osp_ic_read_associated_partners(const osp_value_t *val, osp_associated_partners_t *p) {
	if (!val || !p || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	p->client_sap = osp_get_i8(&val->as.structure.elements.items[0]);
	p->server_sap = osp_get_u16(&val->as.structure.elements.items[1]);
	return OSP_OK;
}

osp_value_t osp_ic_val_threshold(const osp_threshold_t *t) {
	OSP_TLS osp_value_t fields[5];
	osp_value_t v = {0};
	fields[0] = t ? t->normal_value : osp_val_null();
	fields[1] = t ? t->threshold_value : osp_val_null();
	fields[2] = osp_val_u32(t ? t->min_duration : 0);
	fields[3] = osp_val_u32(t ? t->max_duration : 0);
	fields[4] = osp_val_u32(t ? t->emergency_duration : 0);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 5;
	v.as.structure.elements.capacity = 5;
	return v;
}

osp_err_t osp_ic_read_threshold(const osp_value_t *val, osp_threshold_t *t) {
	if (!val || !t || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	t->normal_value = val->as.structure.elements.items[0];
	t->threshold_value = val->as.structure.elements.items[1];
	if (val->as.structure.elements.count > 2) {
		t->min_duration = osp_get_u32(&val->as.structure.elements.items[2]);
	}
	if (val->as.structure.elements.count > 3) {
		t->max_duration = osp_get_u32(&val->as.structure.elements.items[3]);
	}
	if (val->as.structure.elements.count > 4) {
		t->emergency_duration = osp_get_u32(&val->as.structure.elements.items[4]);
	}
	return OSP_OK;
}

osp_value_t osp_ic_val_emergency_profile(const osp_emergency_profile_t *ep) {
	OSP_TLS osp_value_t fields[3];
	osp_value_t v = {0};
	fields[0] = osp_val_i32(ep ? ep->id : 0);
	fields[1] = osp_val_bool(ep ? ep->active : false);
	fields[2].tag = OSP_TAG_OCTETSTRING;
	if (ep) {
		fields[2].as.octetstring.len = 6;
		memcpy(fields[2].as.octetstring.data, &ep->activation_time, 6);
	}
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 3;
	v.as.structure.elements.capacity = 3;
	return v;
}

osp_err_t osp_ic_read_emergency_profile(const osp_value_t *val, osp_emergency_profile_t *ep) {
	if (!val || !ep || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 3) {
		return OSP_ERR_INVALID;
	}
	ep->id = osp_get_i32(&val->as.structure.elements.items[0]);
	ep->active = osp_get_bool(&val->as.structure.elements.items[1]);
	const osp_value_t *ln = &val->as.structure.elements.items[2];
	if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
		return OSP_ERR_INVALID;
	}
	memcpy(&ep->activation_time, ln->as.octetstring.data, 6);
	return OSP_OK;
}

osp_value_t osp_ic_val_capture_object(const osp_capture_object_t *co) {
	OSP_TLS osp_value_t fields[5];
	osp_value_t v = {0};
	fields[0] = osp_val_u16(co ? co->class_id : 0);
	fields[1].tag = OSP_TAG_OCTETSTRING;
	if (co) {
		fields[1].as.octetstring.len = 6;
		memcpy(fields[1].as.octetstring.data, &co->logical_name, 6);
	}
	fields[2] = osp_val_i8(co ? co->attribute_index : 0);
	fields[3] = osp_val_u32(co ? co->data_index : 0);
	fields[4] = osp_val_null();
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 5;
	v.as.structure.elements.capacity = 5;
	return v;
}

osp_err_t osp_ic_read_capture_object(const osp_value_t *val, osp_capture_object_t *co) {
	if (!val || !co || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 4) {
		return OSP_ERR_INVALID;
	}
	co->class_id = osp_get_u16(&val->as.structure.elements.items[0]);
	const osp_value_t *ln = &val->as.structure.elements.items[1];
	if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
		return OSP_ERR_INVALID;
	}
	memcpy(&co->logical_name, ln->as.octetstring.data, 6);
	co->attribute_index = osp_get_i8(&val->as.structure.elements.items[2]);
	co->data_index = osp_get_u32(&val->as.structure.elements.items[3]);
	return OSP_OK;
}

osp_value_t osp_ic_val_threshold_list(const osp_threshold_list_t *tl) {
	OSP_TLS osp_value_t items[16];
	OSP_TLS osp_value_t fields[16][2];
	osp_value_t v = {0};
	uint8_t n = tl ? tl->count : 0;
	if (n > 16) {
		n = 16;
	}
	for (uint8_t i = 0; i < n; i++) {
		fields[i][0] = tl->thresholds[i].value;
		fields[i][1] = osp_val_enum((uint8_t)tl->thresholds[i].severity);
		items[i].tag = OSP_TAG_STRUCTURE;
		items[i].as.structure.elements.items = fields[i];
		items[i].as.structure.elements.count = 2;
		items[i].as.structure.elements.capacity = 2;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = 16;
	return v;
}

osp_err_t osp_ic_read_threshold_list(const osp_value_t *val, osp_threshold_list_t *tl) {
	if (!val || !tl || val->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	tl->count = 0;
	for (uint8_t i = 0; i < val->as.array.elements.count && i < 16; i++) {
		const osp_value_t *row = &val->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
			return OSP_ERR_INVALID;
		}
		tl->thresholds[i].value = row->as.structure.elements.items[0];
		tl->thresholds[i].severity = (osp_severity_t)osp_get_enum(&row->as.structure.elements.items[1]);
		tl->count++;
	}
	return OSP_OK;
}

static osp_value_t ic_val_access_right(const osp_access_right_t *ar) {
	OSP_TLS osp_value_t attr_rows[OSP_MAX_ACCESS_ITEMS];
	OSP_TLS osp_value_t method_rows[OSP_MAX_ACCESS_ITEMS];
	OSP_TLS osp_value_t attr_fields[OSP_MAX_ACCESS_ITEMS][3];
	OSP_TLS osp_value_t method_fields[OSP_MAX_ACCESS_ITEMS][2];
	osp_value_t outer[2];
	osp_value_t v = {0};
	uint8_t ac = ar ? ar->attr_count : 0;
	uint8_t mc = ar ? ar->method_count : 0;
	if (ac > OSP_MAX_ACCESS_ITEMS) {
		ac = OSP_MAX_ACCESS_ITEMS;
	}
	if (mc > OSP_MAX_ACCESS_ITEMS) {
		mc = OSP_MAX_ACCESS_ITEMS;
	}
	for (uint8_t i = 0; i < ac; i++) {
		attr_fields[i][0] = osp_val_i8(ar->attr_items[i].attribute_id);
		attr_fields[i][1] = osp_val_enum((uint8_t)ar->attr_items[i].access_mode);
		attr_fields[i][2] = osp_val_null();
		attr_rows[i].tag = OSP_TAG_STRUCTURE;
		attr_rows[i].as.structure.elements.items = attr_fields[i];
		attr_rows[i].as.structure.elements.count = 3;
		attr_rows[i].as.structure.elements.capacity = 3;
	}
	for (uint8_t i = 0; i < mc; i++) {
		method_fields[i][0] = osp_val_i8(ar->method_items[i].method_id);
		method_fields[i][1] = osp_val_enum((uint8_t)ar->method_items[i].access_mode);
		method_rows[i].tag = OSP_TAG_STRUCTURE;
		method_rows[i].as.structure.elements.items = method_fields[i];
		method_rows[i].as.structure.elements.count = 2;
		method_rows[i].as.structure.elements.capacity = 2;
	}
	outer[0].tag = OSP_TAG_ARRAY;
	outer[0].as.array.elements.items = attr_rows;
	outer[0].as.array.elements.count = ac;
	outer[0].as.array.elements.capacity = OSP_MAX_ACCESS_ITEMS;
	outer[1].tag = OSP_TAG_ARRAY;
	outer[1].as.array.elements.items = method_rows;
	outer[1].as.array.elements.count = mc;
	outer[1].as.array.elements.capacity = OSP_MAX_ACCESS_ITEMS;
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = outer;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

osp_value_t osp_ic_val_object_list(const osp_object_list_t *ol) {
	OSP_TLS osp_value_t rows[OSP_MAX_OBJECT_LIST];
	OSP_TLS osp_value_t fields[OSP_MAX_OBJECT_LIST][4];
	osp_value_t v = {0};
	uint8_t n = ol ? ol->count : 0;
	if (n > OSP_MAX_OBJECT_LIST) {
		n = OSP_MAX_OBJECT_LIST;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_object_list_element_t *e = &ol->elements[i];
		fields[i][0] = osp_val_u16(e->class_id);
		fields[i][1] = osp_val_u8(e->version);
		fields[i][2].tag = OSP_TAG_OCTETSTRING;
		fields[i][2].as.octetstring.len = 6;
		memcpy(fields[i][2].as.octetstring.data, &e->logical_name, 6);
		fields[i][3] = ic_val_access_right(&e->access_rights);
		rows[i].tag = OSP_TAG_STRUCTURE;
		rows[i].as.structure.elements.items = fields[i];
		rows[i].as.structure.elements.count = 4;
		rows[i].as.structure.elements.capacity = 4;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = rows;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = OSP_MAX_OBJECT_LIST;
	return v;
}

osp_err_t osp_ic_read_object_list(const osp_value_t *val, osp_object_list_t *ol) {
	if (!val || !ol || val->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	ol->count = 0;
	for (uint8_t i = 0; i < val->as.array.elements.count && ol->count < OSP_MAX_OBJECT_LIST; i++) {
		const osp_value_t *row = &val->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 4) {
			return OSP_ERR_INVALID;
		}
		osp_object_list_element_t *e = &ol->elements[ol->count];
		e->class_id = osp_get_u16(&row->as.structure.elements.items[0]);
		e->version = osp_get_u8(&row->as.structure.elements.items[1]);
		const osp_value_t *ln = &row->as.structure.elements.items[2];
		if (ln->tag != OSP_TAG_OCTETSTRING || ln->as.octetstring.len != 6) {
			return OSP_ERR_INVALID;
		}
		memcpy(&e->logical_name, ln->as.octetstring.data, 6);
		memset(&e->access_rights, 0, sizeof(e->access_rights));
		ol->count++;
	}
	return OSP_OK;
}

osp_value_t osp_ic_val_user_list_item(const osp_user_list_item_t *item) {
	OSP_TLS osp_value_t fields[2];
	osp_value_t v = {0};
	fields[0] = osp_val_i8(item ? item->id : 0);
	fields[1].tag = OSP_TAG_VISIBLESTRING;
	if (item) {
		fields[1].as.visiblestring.len = item->name_len;
		memcpy(fields[1].as.visiblestring.data, item->name, item->name_len);
		fields[1].as.visiblestring.data[item->name_len] = '\0';
	}
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

osp_err_t osp_ic_read_user_list_item(const osp_value_t *val, osp_user_list_item_t *item) {
	if (!val || !item || val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) {
		return OSP_ERR_INVALID;
	}
	item->id = osp_get_i8(&val->as.structure.elements.items[0]);
	const osp_value_t *name = &val->as.structure.elements.items[1];
	if (name->tag != OSP_TAG_VISIBLESTRING || name->as.visiblestring.len >= OSP_MAX_NAME_LEN) {
		return OSP_ERR_INVALID;
	}
	item->name_len = (uint8_t)name->as.visiblestring.len;
	memcpy(item->name, name->as.visiblestring.data, item->name_len);
	item->name[item->name_len] = '\0';
	return OSP_OK;
}

osp_err_t osp_ic_ln_only_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (attr_id != 1) {
		return OSP_ERR_NOT_FOUND;
	}
	return osp_ic_get_logical_name(result, (const osp_obis_t *)inst);
}

osp_err_t osp_ic_serialize_attrs(const osp_ic_class_t *cls, const void *inst, osp_buf_t *buf, const uint8_t *attr_ids, uint8_t attr_count) {
	if (!cls || !inst || !buf || !attr_ids || !cls->get_attr) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_struct_begin(buf, (uint8_t)(1 + attr_count));
	if (r != OSP_OK) {
		return r;
	}
	r = osp_value_write(buf, &((osp_value_t){.tag = OSP_TAG_LONG_UNSIGNED, .as.uint16 = {.value = cls->class_id}}));
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < attr_count; i++) {
		osp_value_t v;
		r = cls->get_attr(inst, attr_ids[i], &v);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_value_write(buf, &v);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_ic_deserialize_attrs(const osp_ic_class_t *cls, void *inst, osp_buf_t *buf, const uint8_t *attr_ids, uint8_t attr_count) {
	if (!cls || !inst || !buf || !attr_ids) {
		return OSP_ERR_INVALID;
	}

	uint8_t nf;
	osp_err_t r = osp_struct_begin_read(buf, &nf);
	if (r != OSP_OK || nf != (uint8_t)(1 + attr_count)) {
		return OSP_ERR_INVALID;
	}

	osp_value_t field;
	r = osp_value_read(buf, &field);
	if (r != OSP_OK || field.tag != OSP_TAG_LONG_UNSIGNED || field.as.uint16.value != cls->class_id) {
		return OSP_ERR_INVALID;
	}

	for (uint8_t i = 0; i < attr_count; i++) {
		r = osp_value_read(buf, &field);
		if (r != OSP_OK) {
			return r;
		}
		if (attr_ids[i] == 1) {
			r = osp_ic_set_logical_name((osp_obis_t *)inst, &field);
		} else if (cls->set_attr) {
			r = cls->set_attr(inst, attr_ids[i], &field);
		} else {
			r = OSP_ERR_UNSUPPORTED;
		}
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

/* ── context_name serialization ─────────────────────────────────────────── */

osp_value_t osp_ic_val_context_name(const osp_context_name_t *cn) {
	osp_value_t v = {0};
	if (!cn) return v;

	if (cn->is_structure) {
		OSP_TLS osp_value_t fields[7];
		fields[0] = osp_val_u8(cn->as.structure.joint_iso_ctt);
		fields[1] = osp_val_u8(cn->as.structure.country);
		fields[2] = osp_val_u16(cn->as.structure.country_name);
		fields[3] = osp_val_u8(cn->as.structure.identified_organization);
		fields[4] = osp_val_u8(cn->as.structure.dlms_ua);
		fields[5] = osp_val_u8(cn->as.structure.application_context);
		fields[6] = osp_val_u8(cn->as.structure.context_id);
		v.tag = OSP_TAG_STRUCTURE;
		v.as.structure.elements.items = fields;
		v.as.structure.elements.count = 7;
		v.as.structure.elements.capacity = 7;
	} else {
		v.tag = OSP_TAG_OCTETSTRING;
		v.as.octetstring.len = cn->as.oid.len;
		memcpy(v.as.octetstring.data, cn->as.oid.data, cn->as.oid.len);
	}
	return v;
}

osp_err_t osp_ic_read_context_name(const osp_value_t *val, osp_context_name_t *cn) {
	if (!val || !cn) return OSP_ERR_INVALID;

	if (val->tag == OSP_TAG_STRUCTURE) {
		cn->is_structure = true;
		if (val->as.structure.elements.count >= 7) {
			const osp_value_t *items = val->as.structure.elements.items;
			cn->as.structure.joint_iso_ctt = osp_get_u8(&items[0]);
			cn->as.structure.country = osp_get_u8(&items[1]);
			cn->as.structure.country_name = osp_get_u16(&items[2]);
			cn->as.structure.identified_organization = osp_get_u8(&items[3]);
			cn->as.structure.dlms_ua = osp_get_u8(&items[4]);
			cn->as.structure.application_context = osp_get_u8(&items[5]);
			cn->as.structure.context_id = osp_get_u8(&items[6]);
		}
	} else if (val->tag == OSP_TAG_OCTETSTRING) {
		cn->is_structure = false;
		cn->as.oid.len = val->as.octetstring.len;
		if (cn->as.oid.len > sizeof(cn->as.oid.data)) cn->as.oid.len = sizeof(cn->as.oid.data);
		memcpy(cn->as.oid.data, val->as.octetstring.data, cn->as.oid.len);
	} else {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

/* ── Activity Calendar types serialization ──────────────────────────────── */

osp_value_t osp_ic_val_season(const osp_season_t *s) {
	osp_value_t v = {0};
	if (!s) return v;
	OSP_TLS osp_value_t fields[3];
	fields[0].tag = OSP_TAG_OCTETSTRING;
	fields[0].as.octetstring.len = s->name_len;
	memcpy(fields[0].as.octetstring.data, s->name, s->name_len);
	fields[1].tag = OSP_TAG_OCTETSTRING;
	uint8_t dt[12];
	memcpy(dt, &s->start, sizeof(osp_datetime_t));
	fields[1].as.octetstring.len = 12;
	memcpy(fields[1].as.octetstring.data, dt, 12);
	fields[2].tag = OSP_TAG_OCTETSTRING;
	fields[2].as.octetstring.len = s->week_name_len;
	memcpy(fields[2].as.octetstring.data, s->week_name, s->week_name_len);
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 3;
	v.as.structure.elements.capacity = 3;
	return v;
}

osp_err_t osp_ic_read_season(const osp_value_t *val, osp_season_t *s) {
	if (!val || !s) return OSP_ERR_INVALID;
	if (val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 3) return OSP_ERR_INVALID;
	const osp_value_t *items = val->as.structure.elements.items;
	if (items[0].tag == OSP_TAG_OCTETSTRING) {
		s->name_len = items[0].as.octetstring.len;
		if (s->name_len > OSP_MAX_NAME_LEN) s->name_len = OSP_MAX_NAME_LEN;
		memcpy(s->name, items[0].as.octetstring.data, s->name_len);
	}
	if (items[1].tag == OSP_TAG_OCTETSTRING && items[1].as.octetstring.len >= sizeof(osp_datetime_t)) {
		memcpy(&s->start, items[1].as.octetstring.data, sizeof(osp_datetime_t));
	}
	if (items[2].tag == OSP_TAG_OCTETSTRING) {
		s->week_name_len = items[2].as.octetstring.len;
		if (s->week_name_len > OSP_MAX_NAME_LEN) s->week_name_len = OSP_MAX_NAME_LEN;
		memcpy(s->week_name, items[2].as.octetstring.data, s->week_name_len);
	}
	return OSP_OK;
}

osp_value_t osp_ic_val_week_profile(const osp_week_profile_t *wp) {
	osp_value_t v = {0};
	if (!wp) return v;
	OSP_TLS osp_value_t fields[8];
	fields[0].tag = OSP_TAG_OCTETSTRING;
	fields[0].as.octetstring.len = wp->name_len;
	memcpy(fields[0].as.octetstring.data, wp->name, wp->name_len);
	for (int d = 0; d < 7; d++) {
		fields[1 + d] = osp_val_u8(wp->day_names[d][0]);
	}
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 8;
	v.as.structure.elements.capacity = 8;
	return v;
}

osp_err_t osp_ic_read_week_profile(const osp_value_t *val, osp_week_profile_t *wp) {
	if (!val || !wp) return OSP_ERR_INVALID;
	if (val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 8) return OSP_ERR_INVALID;
	const osp_value_t *items = val->as.structure.elements.items;
	if (items[0].tag == OSP_TAG_OCTETSTRING) {
		wp->name_len = items[0].as.octetstring.len;
		if (wp->name_len > OSP_MAX_NAME_LEN) wp->name_len = OSP_MAX_NAME_LEN;
		memcpy(wp->name, items[0].as.octetstring.data, wp->name_len);
	}
	for (int d = 0; d < 7; d++) {
		wp->day_names[d][0] = osp_get_u8(&items[1 + d]);
		wp->day_name_lens[d] = 1;
	}
	return OSP_OK;
}

osp_value_t osp_ic_val_day_profile(const osp_day_profile_t *dp) {
	osp_value_t v = {0};
	if (!dp) return v;
	OSP_TLS osp_value_t fields[2];
	fields[0].tag = OSP_TAG_OCTETSTRING;
	fields[0].as.octetstring.len = dp->name_len;
	memcpy(fields[0].as.octetstring.data, dp->name, dp->name_len);
	fields[1].tag = OSP_TAG_UNSIGNED;
	fields[1].as.uint8.value = dp->action_count;
	v.tag = OSP_TAG_STRUCTURE;
	v.as.structure.elements.items = fields;
	v.as.structure.elements.count = 2;
	v.as.structure.elements.capacity = 2;
	return v;
}

osp_err_t osp_ic_read_day_profile(const osp_value_t *val, osp_day_profile_t *dp) {
	if (!val || !dp) return OSP_ERR_INVALID;
	if (val->tag != OSP_TAG_STRUCTURE || val->as.structure.elements.count < 2) return OSP_ERR_INVALID;
	const osp_value_t *items = val->as.structure.elements.items;
	if (items[0].tag == OSP_TAG_OCTETSTRING) {
		dp->name_len = items[0].as.octetstring.len;
		if (dp->name_len > OSP_MAX_NAME_LEN) dp->name_len = OSP_MAX_NAME_LEN;
		memcpy(dp->name, items[0].as.octetstring.data, dp->name_len);
	}
	dp->action_count = osp_get_u8(&items[1]);
	if (dp->action_count > OSP_MAX_DAY_ACTION) dp->action_count = OSP_MAX_DAY_ACTION;
	return OSP_OK;
}
