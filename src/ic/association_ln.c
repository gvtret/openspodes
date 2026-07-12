#include "association_ln.h"
#include "ic_common.h"
#include <string.h>

/* ── Helper: match OBIS + class_id in object_list ────────────────────────── */

static int find_index(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln) {
	for (uint8_t i = 0; i < a->object_list.count; i++) {
		const osp_object_list_element_t *e = &a->object_list.elements[i];
		if (e->class_id == class_id && osp_obis_eq(&e->logical_name, ln)) {
			return i;
		}
	}
	return -1;
}

/* ── Object list management ──────────────────────────────────────────────── */

osp_err_t osp_ic_association_ln_add_object(osp_ic_association_ln_t *a, const osp_object_list_element_t *elem) {
	if (!a || !elem || a->object_list.count >= OSP_MAX_OBJECT_LIST_ENTRIES) {
		return OSP_ERR_NOMEM;
	}
	a->object_list.elements[a->object_list.count] = *elem;
	a->object_list.count++;
	return OSP_OK;
}

osp_err_t osp_ic_association_ln_remove_object(osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln) {
	if (!a || !ln) {
		return OSP_ERR_INVALID;
	}

	int idx = find_index(a, class_id, ln);
	if (idx < 0) {
		return OSP_ERR_NOT_FOUND;
	}

	/* Shift remaining entries down */
	for (uint8_t i = (uint8_t)idx; i + 1 < a->object_list.count; i++) {
		a->object_list.elements[i] = a->object_list.elements[i + 1];
	}
	a->object_list.count--;
	return OSP_OK;
}

const osp_object_list_element_t *osp_ic_association_ln_find_object(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln) {
	if (!a || !ln) {
		return NULL;
	}

	int idx = find_index(a, class_id, ln);
	return (idx >= 0) ? &a->object_list.elements[idx] : NULL;
}

bool osp_ic_association_ln_can_read(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id) {
	const osp_object_list_element_t *e = osp_ic_association_ln_find_object(a, class_id, ln);
	if (!e) {
		return false;
	}
	for (uint8_t i = 0; i < e->access_rights.attr_count; i++) {
		if (e->access_rights.attr_items[i].attribute_id == attr_id) {
			return (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_READ_ONLY) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_READ_WRITE) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_AUTH_READ_ONLY) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_AUTH_READ_WRITE);
		}
	}
	return false;
}

bool osp_ic_association_ln_can_write(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id) {
	const osp_object_list_element_t *e = osp_ic_association_ln_find_object(a, class_id, ln);
	if (!e) {
		return false;
	}
	for (uint8_t i = 0; i < e->access_rights.attr_count; i++) {
		if (e->access_rights.attr_items[i].attribute_id == attr_id) {
			return (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_WRITE_ONLY) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_READ_WRITE) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_AUTH_WRITE_ONLY) ||
			    (e->access_rights.attr_items[i].access_mode == OSP_ACCESS_AUTH_READ_WRITE);
		}
	}
	return false;
}

bool osp_ic_association_ln_can_invoke(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t method_id) {
	const osp_object_list_element_t *e = osp_ic_association_ln_find_object(a, class_id, ln);
	if (!e) {
		return false;
	}
	for (uint8_t i = 0; i < e->access_rights.method_count; i++) {
		if (e->access_rights.method_items[i].method_id == method_id) {
			return (e->access_rights.method_items[i].access_mode != OSP_METHOD_NO_ACCESS);
		}
	}
	return false;
}

/* ── Vtable: get/set/invoke ──────────────────────────────────────────────── */

static const uint8_t aln_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static osp_value_t aln_context_name_value(const osp_context_name_t *cn) {
	static osp_value_t fields[7];
	osp_value_t v = {0};
	if (cn && cn->is_structure) {
		const osp_context_name_structure_t *s = &cn->as.structure;
		fields[0] = osp_val_u8(s->joint_iso_ctt);
		fields[1] = osp_val_u8(s->country);
		fields[2] = osp_val_u16(s->country_name);
		fields[3] = osp_val_u8(s->identified_organization);
		fields[4] = osp_val_u8(s->dlms_ua);
		fields[5] = osp_val_u8(s->application_context);
		fields[6] = osp_val_u8(s->context_id);
		v.tag = OSP_TAG_STRUCTURE;
		v.as.structure.elements.items = fields;
		v.as.structure.elements.count = 7;
		v.as.structure.elements.capacity = 7;
		return v;
	}
	v.tag = OSP_TAG_OCTETSTRING;
	if (cn) {
		v.as.octetstring.len = cn->as.oid.len;
		memcpy(v.as.octetstring.data, cn->as.oid.data, cn->as.oid.len);
	}
	return v;
}

static osp_err_t aln_read_context_name(const osp_value_t *value, osp_context_name_t *cn) {
	if (!value || !cn) {
		return OSP_ERR_INVALID;
	}
	if (value->tag == OSP_TAG_STRUCTURE && value->as.structure.elements.count >= 7) {
		cn->is_structure = true;
		osp_context_name_structure_t *s = &cn->as.structure;
		s->joint_iso_ctt = osp_get_u8(&value->as.structure.elements.items[0]);
		s->country = osp_get_u8(&value->as.structure.elements.items[1]);
		s->country_name = osp_get_u16(&value->as.structure.elements.items[2]);
		s->identified_organization = osp_get_u8(&value->as.structure.elements.items[3]);
		s->dlms_ua = osp_get_u8(&value->as.structure.elements.items[4]);
		s->application_context = osp_get_u8(&value->as.structure.elements.items[5]);
		s->context_id = osp_get_u8(&value->as.structure.elements.items[6]);
		return OSP_OK;
	}
	if (value->tag == OSP_TAG_OCTETSTRING) {
		cn->is_structure = false;
		cn->as.oid.len = (uint8_t)value->as.octetstring.len;
		if (cn->as.oid.len > sizeof(cn->as.oid.data)) {
			cn->as.oid.len = sizeof(cn->as.oid.data);
		}
		memcpy(cn->as.oid.data, value->as.octetstring.data, cn->as.oid.len);
		return OSP_OK;
	}
	return OSP_ERR_INVALID;
}

static osp_value_t aln_user_list_value(const osp_ic_association_ln_t *a) {
	static osp_value_t items[16];
	osp_value_t v = {0};
	for (uint8_t i = 0; i < a->user_count && i < 16; i++) {
		items[i] = osp_ic_val_user_list_item(&a->user_list[i]);
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = a->user_count;
	v.as.array.elements.capacity = 16;
	return v;
}

static osp_err_t aln_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_association_ln_t *a = (const osp_ic_association_ln_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
			*result = osp_ic_val_object_list(&a->object_list);
			return OSP_OK;
		case 3:
			*result = osp_ic_val_associated_partners(&a->associated_partners);
			return OSP_OK;
		case 4:
			*result = aln_context_name_value(&a->app_context_name);
			return OSP_OK;
		case 5:
			*result = osp_ic_val_xdms_context(&a->xdms_context);
			return OSP_OK;
		case 6:
			*result = osp_val_enum(a->authentication_mechanism);
			return OSP_OK;
		case 7:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = a->secret_len;
			memcpy(result->as.octetstring.data, a->secret, a->secret_len);
			return OSP_OK;
		case 8:
			*result = osp_val_enum(a->association_status);
			return OSP_OK;
		case 9:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = 6;
			memcpy(result->as.octetstring.data, &a->security_setup_reference, 6);
			return OSP_OK;
		case 10:
			*result = aln_user_list_value(a);
			return OSP_OK;
		case 11:
			*result = osp_ic_val_user_list_item(&a->current_user);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t aln_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return osp_ic_read_object_list(value, &a->object_list);
		case 3:
			return osp_ic_read_associated_partners(value, &a->associated_partners);
		case 4:
			return aln_read_context_name(value, &a->app_context_name);
		case 5:
			return osp_ic_read_xdms_context(value, &a->xdms_context);
		case 6:
			a->authentication_mechanism = osp_get_enum(value);
			return OSP_OK;
		case 7:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len > sizeof(a->secret)) {
				return OSP_ERR_INVALID;
			}
			a->secret_len = (uint8_t)value->as.octetstring.len;
			memcpy(a->secret, value->as.octetstring.data, a->secret_len);
			return OSP_OK;
		case 8:
			a->association_status = osp_get_enum(value);
			return OSP_OK;
		case 9:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 6) {
				return OSP_ERR_INVALID;
			}
			memcpy(&a->security_setup_reference, value->as.octetstring.data, 6);
			return OSP_OK;
		case 10:
			return OSP_OK;
		case 11:
			return osp_ic_read_user_list_item(value, &a->current_user);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t aln_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	(void)inst;
	(void)param;
	*result = osp_val_null();

	switch (method_id) {
		case 1: /* reply_to_HLS_authentication — handled by channel, not here */
			return OSP_ERR_NOT_FOUND;
		case 3: /* add_object */
			return OSP_OK;
		case 4: /* remove_object */
			return OSP_OK;
		case 5: /* add_user */
			return OSP_OK;
		case 6: /* remove_user */
			return OSP_OK;
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t aln_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_association_ln_class(), inst, buf, aln_attrs, 11);
}

static osp_err_t aln_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_association_ln_class(), inst, buf, aln_attrs, 11);
}

/* ── Class definition ────────────────────────────────────────────────────── */

static const osp_ic_class_t ic_aln = {
    .name = "Association LN",
    .class_id = 15,
    .version = 2,
    .get_attr = aln_get,
    .set_attr = aln_set,
    .invoke = aln_invoke,
    .serialize = aln_serialize,
    .deserialize = aln_deserialize,
    .instance_size = sizeof(osp_ic_association_ln_t),
};

const osp_ic_class_t *osp_ic_association_ln_class(void) {
	return &ic_aln;
}

void osp_ic_association_ln_init(osp_ic_association_ln_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
	a->association_status = 0;               /* idle */
	a->associated_partners.client_sap = 16;
	a->associated_partners.server_sap = 1;
	a->app_context_name.is_structure = true;
	a->app_context_name.as.structure.joint_iso_ctt = 2;
	a->app_context_name.as.structure.country = 16;
	a->app_context_name.as.structure.country_name = 756;
	a->app_context_name.as.structure.identified_organization = 5;
	a->app_context_name.as.structure.dlms_ua = 8;
	a->app_context_name.as.structure.application_context = 1;
	a->app_context_name.as.structure.context_id = 1;
	a->xdms_context.dlms_version_number = 6; /* DLMS/COSEM edition 6 */
	a->xdms_context.max_receive_pdu_size = 1024;
	a->xdms_context.max_send_pdu_size = 1024;
}
