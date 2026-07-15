#include "sap_assignment.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t sap_attrs[] = {1, 2};

static osp_value_t sap_list_value(const osp_ic_sap_assignment_t *s) {
	OSP_TLS osp_value_t row_fields[16][2];
	OSP_TLS osp_value_t rows[16];
	osp_value_t v = {0};
	for (uint8_t i = 0; i < s->sap_list.count && i < 16; i++) {
		row_fields[i][0] = osp_val_u16(s->sap_list.items[i].sap);
		row_fields[i][1].tag = OSP_TAG_OCTETSTRING;
		row_fields[i][1].as.octetstring.len = s->sap_list.items[i].logical_device_name_len;
		memcpy(row_fields[i][1].as.octetstring.data, s->sap_list.items[i].logical_device_name,
		       s->sap_list.items[i].logical_device_name_len);
		rows[i].tag = OSP_TAG_STRUCTURE;
		rows[i].as.structure.elements.items = row_fields[i];
		rows[i].as.structure.elements.count = 2;
		rows[i].as.structure.elements.capacity = 2;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = rows;
	v.as.array.elements.count = s->sap_list.count;
	v.as.array.elements.capacity = 16;
	return v;
}

static osp_err_t sap_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_sap_assignment_t *s = (const osp_ic_sap_assignment_t *)inst;
	if (attr_id == 1) {
		return osp_ic_get_logical_name(result, &s->logical_name);
	}
	if (attr_id == 2) {
		*result = sap_list_value(s);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static int sap_find(const osp_ic_sap_assignment_t *s, uint16_t sap) {
	for (uint8_t i = 0; i < s->sap_list.count; i++) {
		if (s->sap_list.items[i].sap == sap) {
			return (int)i;
		}
	}
	return -1;
}

static osp_err_t sap_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_sap_assignment_t *s = (osp_ic_sap_assignment_t *)inst;
	if (!value || attr_id != 2 || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_NOT_FOUND;
	}
	s->sap_list.count = 0;
	for (uint8_t i = 0; i < value->as.array.elements.count && s->sap_list.count < 16; i++) {
		const osp_value_t *row = &value->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
			continue;
		}
		osp_sap_assignment_item_t *item = &s->sap_list.items[s->sap_list.count++];
		item->sap = osp_get_u16(&row->as.structure.elements.items[0]);
		const osp_value_t *ldn = &row->as.structure.elements.items[1];
		if (ldn->tag == OSP_TAG_OCTETSTRING) {
			uint32_t len = ldn->as.octetstring.len;
			if (len > sizeof(item->logical_device_name)) {
				len = sizeof(item->logical_device_name);
			}
			item->logical_device_name_len = (uint8_t)len;
			memcpy(item->logical_device_name, ldn->as.octetstring.data, len);
		}
	}
	return OSP_OK;
}

static osp_err_t sap_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_sap_assignment_t *s = (osp_ic_sap_assignment_t *)inst;
	if (method_id != 1 || !param || param->tag != OSP_TAG_STRUCTURE || param->as.structure.elements.count < 2) {
		return OSP_ERR_UNSUPPORTED;
	}
	uint16_t sap = osp_get_u16(&param->as.structure.elements.items[0]);
	const osp_value_t *ldn = &param->as.structure.elements.items[1];
	int idx = sap_find(s, sap);
	if (ldn->tag == OSP_TAG_OCTETSTRING && ldn->as.octetstring.len > 0) {
		if (idx >= 0) {
			osp_sap_assignment_item_t *item = &s->sap_list.items[idx];
			item->logical_device_name_len = (uint8_t)ldn->as.octetstring.len;
			memcpy(item->logical_device_name, ldn->as.octetstring.data, ldn->as.octetstring.len);
		} else if (s->sap_list.count < 16) {
			osp_sap_assignment_item_t *item = &s->sap_list.items[s->sap_list.count++];
			item->sap = sap;
			item->logical_device_name_len = (uint8_t)ldn->as.octetstring.len;
			memcpy(item->logical_device_name, ldn->as.octetstring.data, ldn->as.octetstring.len);
		} else {
			return OSP_ERR_NOMEM;
		}
	} else if (idx >= 0) {
		for (uint8_t i = (uint8_t)idx; i + 1 < s->sap_list.count; i++) {
			s->sap_list.items[i] = s->sap_list.items[i + 1];
		}
		s->sap_list.count--;
	}
	if (result) {
		*result = osp_val_null();
	}
	return OSP_OK;
}

static osp_err_t sap_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_sap_assignment_class(), inst, buf, sap_attrs, 2);
}

static osp_err_t sap_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_sap_assignment_class(), inst, buf, sap_attrs, 2);
}

static const osp_ic_class_t ic_sap = {
    .name = "SAP Assignment",
    .class_id = 17,
    .version = 0,
    .get_attr = sap_get,
    .set_attr = sap_set,
    .invoke = sap_invoke,
    .serialize = sap_serialize,
    .deserialize = sap_deserialize,
    .instance_size = sizeof(osp_ic_sap_assignment_t),
};

const osp_ic_class_t *osp_ic_sap_assignment_class(void) {
	return &ic_sap;
}

void osp_ic_sap_assignment_init(osp_ic_sap_assignment_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
