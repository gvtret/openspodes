#include "data_protection.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t dp_attrs[] = {1, 2};

static osp_value_t dp_list_value(const osp_ic_data_protection_t *d) {
	OSP_TLS osp_value_t rows[8];
	OSP_TLS osp_value_t fields[8][2];
	osp_value_t v = {0};
	for (uint8_t i = 0; i < d->protection_methods.count && i < 8; i++) {
		fields[i][0] = osp_val_enum((uint8_t)d->protection_methods.entries[i].method);
		fields[i][1] = osp_val_u8(d->protection_methods.entries[i].global_key_list_id);
		rows[i].tag = OSP_TAG_STRUCTURE;
		rows[i].as.structure.elements.items = fields[i];
		rows[i].as.structure.elements.count = 2;
		rows[i].as.structure.elements.capacity = 2;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = rows;
	v.as.array.elements.count = d->protection_methods.count;
	v.as.array.elements.capacity = 8;
	return v;
}

static osp_err_t dp_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_data_protection_t *d = (const osp_ic_data_protection_t *)inst;
	if (attr_id == 1) {
		return osp_ic_get_logical_name(result, &d->logical_name);
	}
	if (attr_id == 2) {
		*result = dp_list_value(d);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t dp_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_data_protection_t *d = (osp_ic_data_protection_t *)inst;
	if (!value || attr_id != 2 || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_NOT_FOUND;
	}
	d->protection_methods.count = 0;
	for (uint8_t i = 0; i < value->as.array.elements.count && d->protection_methods.count < 8; i++) {
		const osp_value_t *row = &value->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 2) {
			continue;
		}
		osp_data_protection_entry_t *e = &d->protection_methods.entries[d->protection_methods.count++];
		e->method = (osp_data_protection_method_t)osp_get_enum(&row->as.structure.elements.items[0]);
		e->global_key_list_id = osp_get_u8(&row->as.structure.elements.items[1]);
	}
	return OSP_OK;
}

static osp_err_t dp_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_data_protection_class(), inst, buf, dp_attrs, 2);
}

static osp_err_t dp_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_data_protection_class(), inst, buf, dp_attrs, 2);
}

static const osp_ic_class_t ic_dp = {
    .name = "Data Protection",
    .class_id = 30,
    .version = 0,
    .get_attr = dp_get,
    .set_attr = dp_set,
    .invoke = NULL,
    .serialize = dp_serialize,
    .deserialize = dp_deserialize,
    .instance_size = sizeof(osp_ic_data_protection_t),
};

const osp_ic_class_t *osp_ic_data_protection_class(void) {
	return &ic_dp;
}

void osp_ic_data_protection_init(osp_ic_data_protection_t *d, osp_obis_t ln) {
	memset(d, 0, sizeof(*d));
	d->logical_name = ln;
}
