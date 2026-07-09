#include "association_ln.h"
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

static osp_err_t aln_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_association_ln_t *a = (const osp_ic_association_ln_t *)inst;
	switch (attr_id) {
		case 2:
			/* object_list: return count as placeholder */
			*result = osp_val_u32(a->object_list.count);
			return OSP_OK;
		case 5:
			/* xDLMS context: return max_receive as representative */
			*result = osp_val_u16(a->xdms_context.max_receive_pdu_size);
			return OSP_OK;
		case 8:
			/* association_status */
			*result = osp_val_u8(a->association_status);
			return OSP_OK;
		case 10:
			/* user_list: return count */
			*result = osp_val_u8(a->user_count);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t aln_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)inst;
	(void)a;
	(void)attr_id;
	(void)value;
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t aln_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)inst;
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

/* ── Class definition ────────────────────────────────────────────────────── */

static const osp_ic_class_t ic_aln = {
    .name = "Association LN",
    .class_id = 15,
    .version = 2,
    .get_attr = aln_get,
    .set_attr = aln_set,
    .invoke = aln_invoke,
    .instance_size = sizeof(osp_ic_association_ln_t),
};

const osp_ic_class_t *osp_ic_association_ln_class(void) {
	return &ic_aln;
}

void osp_ic_association_ln_init(osp_ic_association_ln_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
	a->association_status = 0;               /* idle */
	a->xdms_context.dlms_version_number = 6; /* DLMS/COSEM edition 6 */
	a->xdms_context.max_receive_pdu_size = 1024;
}
