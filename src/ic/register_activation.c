#include "register_activation.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ra_attrs[] = {1, 2, 3, 4};

static osp_value_t ra_register_assignment_val(const osp_ic_register_activation_t *a) {
	OSP_TLS osp_u32_array_view_t view;
	osp_value_t v = {0};
	uint8_t n = a->register_assignment.count;
	if (n > OSP_MAX_MASK_REGISTERS) {
		n = OSP_MAX_MASK_REGISTERS;
	}
	if (n == 0) {
		return osp_ic_val_empty_array();
	}
	view.items = a->register_assignment.indices;
	view.count = n;
	view.max_count = OSP_MAX_MASK_REGISTERS;
	v.tag = OSP_TAG_U32_ARRAY_REF;
	v.as.ref = &view;
	return v;
}

static osp_value_t ra_mask_list_val(const osp_ic_register_activation_t *a) {
	osp_value_t v = {0};
	if (a->mask_list.count == 0) {
		return osp_ic_val_empty_array();
	}
	v.tag = OSP_TAG_MASK_LIST_REF;
	v.as.ref = &a->mask_list;
	return v;
}

static osp_err_t ra_read_register_assignment(const osp_value_t *value, osp_register_list_t *list) {
	if (!value || !list || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = value->as.array.elements.count;
	if (n > OSP_MAX_MASK_REGISTERS) {
		n = OSP_MAX_MASK_REGISTERS;
	}
	for (uint8_t i = 0; i < n; i++) {
		list->indices[i] = osp_get_u32(&value->as.array.elements.items[i]);
	}
	list->count = n;
	return OSP_OK;
}

static osp_err_t ra_read_mask_list(const osp_value_t *value, osp_mask_list_t *list) {
	if (!value || !list || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = value->as.array.elements.count;
	if (n > OSP_MAX_MASK_LIST) {
		n = OSP_MAX_MASK_LIST;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_value_t *row = &value->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
			return OSP_ERR_INVALID;
		}
		osp_activation_mask_t *m = &list->items[i];
		memset(m, 0, sizeof(*m));
		const osp_value_t *name = &row->as.structure.elements.items[0];
		if (name->tag == OSP_TAG_OCTETSTRING) {
			m->mask_name_len = name->as.octetstring.len;
			if (m->mask_name_len > OSP_MAX_NAME_LEN) {
				m->mask_name_len = OSP_MAX_NAME_LEN;
			}
			memcpy(m->mask_name, name->as.octetstring.data, m->mask_name_len);
		}
		const osp_value_t *idxs = &row->as.structure.elements.items[1];
		if (idxs->tag != OSP_TAG_ARRAY) {
			return OSP_ERR_INVALID;
		}
		uint8_t in = idxs->as.array.elements.count;
		if (in > OSP_MAX_MASK_REGISTERS) {
			in = OSP_MAX_MASK_REGISTERS;
		}
		m->register_list.count = in;
		for (uint8_t j = 0; j < in; j++) {
			m->register_list.indices[j] = osp_get_u32(&idxs->as.array.elements.items[j]);
		}
	}
	list->count = n;
	return OSP_OK;
}

static osp_err_t ra_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_register_activation_t *a = (const osp_ic_register_activation_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
			*result = ra_register_assignment_val(a);
			return OSP_OK;
		case 3:
			*result = ra_mask_list_val(a);
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_OCTETSTRING;
			{
				uint16_t len = (uint16_t)a->active_mask_bits;
				if (len > sizeof(a->active_mask)) {
					len = sizeof(a->active_mask);
				}
				result->as.octetstring.len = len;
				memcpy(result->as.octetstring.data, a->active_mask, len);
			}
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ra_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_register_activation_t *a = (osp_ic_register_activation_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return ra_read_register_assignment(value, &a->register_assignment);
		case 3:
			return ra_read_mask_list(value, &a->mask_list);
		case 4:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len > sizeof(a->active_mask)) {
				return OSP_ERR_INVALID;
			}
			memset(a->active_mask, 0, sizeof(a->active_mask));
			memcpy(a->active_mask, value->as.octetstring.data, value->as.octetstring.len);
			a->active_mask_bits = value->as.octetstring.len;
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ra_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_register_activation_t *a = (osp_ic_register_activation_t *)inst;
	*result = osp_val_null();

	switch (method_id) {
		case 1: { /* add_register: append unsigned/long-unsigned index */
			if (!param || a->register_assignment.count >= OSP_MAX_MASK_REGISTERS) {
				return OSP_ERR_INVALID;
			}
			a->register_assignment.indices[a->register_assignment.count++] = osp_get_u32(param);
			return OSP_OK;
		}
		case 2: { /* add_mask: structure { name, index_list } */
			if (!param || param->tag != OSP_TAG_STRUCTURE || param->as.structure.elements.count < 2) {
				return OSP_ERR_INVALID;
			}
			if (a->mask_list.count >= OSP_MAX_MASK_LIST) {
				return OSP_ERR_INVALID;
			}
			osp_value_t items[1];
			items[0] = *param;
			osp_value_t arr = {0};
			arr.tag = OSP_TAG_ARRAY;
			arr.as.array.elements.items = items;
			arr.as.array.elements.count = 1;
			osp_mask_list_t tmp = {0};
			if (ra_read_mask_list(&arr, &tmp) != OSP_OK) {
				return OSP_ERR_INVALID;
			}
			a->mask_list.items[a->mask_list.count++] = tmp.items[0];
			return OSP_OK;
		}
		case 3: { /* delete_mask by name */
			if (!param || param->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			for (uint8_t i = 0; i < a->mask_list.count; i++) {
				osp_activation_mask_t *m = &a->mask_list.items[i];
				if (m->mask_name_len == param->as.octetstring.len &&
				    memcmp(m->mask_name, param->as.octetstring.data, m->mask_name_len) == 0) {
					for (uint8_t j = i; j + 1 < a->mask_list.count; j++) {
						a->mask_list.items[j] = a->mask_list.items[j + 1];
					}
					a->mask_list.count--;
					return OSP_OK;
				}
			}
			return OSP_ERR_NOT_FOUND;
		}
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t ra_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_register_activation_class(), inst, buf, ra_attrs, 4);
}

static osp_err_t ra_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_register_activation_class(), inst, buf, ra_attrs, 4);
}

static const osp_ic_class_t ic_ra = {
    .name = "Register Activation",
    .class_id = 6,
    .version = 0,
    .get_attr = ra_get,
    .set_attr = ra_set,
    .invoke = ra_invoke,
    .serialize = ra_serialize,
    .deserialize = ra_deserialize,
    .instance_size = sizeof(osp_ic_register_activation_t),
};

const osp_ic_class_t *osp_ic_register_activation_class(void) {
	return &ic_ra;
}

void osp_ic_register_activation_init(osp_ic_register_activation_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
