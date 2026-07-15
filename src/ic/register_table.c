#include "register_table.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t rt_attrs[] = {1, 2, 3, 4};

static osp_err_t rt_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_register_table_t *i = (const osp_ic_register_table_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
		case 2: {
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.items = (osp_value_t *)i->table_cell_values;
			result->as.array.elements.count = i->table_cell_count;
			result->as.array.elements.capacity = OSP_MAX_REGISTER_TABLE;
			return OSP_OK;
		}
		case 3: {
			OSP_TLS osp_value_t fields[4];
			const osp_table_cell_t *tc = &i->table_cell_definition;
			fields[0] = osp_val_u16(tc->class_id);
			fields[1].tag = OSP_TAG_OCTETSTRING;
			fields[1].as.octetstring.len = 6;
			memcpy(fields[1].as.octetstring.data, &tc->logical_name, 6);
			fields[2] = osp_val_i8(tc->attribute_index);
			fields[3] = osp_val_null();
			result->tag = OSP_TAG_STRUCTURE;
			result->as.structure.elements.items = fields;
			result->as.structure.elements.count = 4;
			result->as.structure.elements.capacity = 4;
			return OSP_OK;
		}
		case 4:
			*result = osp_ic_val_scaler_unit(&i->scaler_unit);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t rt_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_register_table_t *i = (osp_ic_register_table_t *)inst;
	if (!value) return OSP_ERR_INVALID;
	switch (attr_id) {
		case 2: {
			/* Store table_cell_values from incoming array */
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			i->table_cell_count = value->as.array.elements.count;
			if (i->table_cell_count > OSP_MAX_REGISTER_TABLE) {
				i->table_cell_count = OSP_MAX_REGISTER_TABLE;
			}
			for (uint8_t idx = 0; idx < i->table_cell_count; idx++) {
				i->table_cell_values[idx] = value->as.array.elements.items[idx];
			}
			return OSP_OK;
		}
		case 3: {
			/* Store table_cell_definition (4-field structure) */
			if (value->tag != OSP_TAG_STRUCTURE || value->as.structure.elements.count < 4) {
				return OSP_ERR_INVALID;
			}
			const osp_value_t *elems = value->as.structure.elements.items;
			i->table_cell_definition.class_id = elems[0].as.uint16.value;
			if (elems[1].tag == OSP_TAG_OCTETSTRING) {
				memcpy(&i->table_cell_definition.logical_name, elems[1].as.octetstring.data, sizeof(osp_obis_t));
			}
			i->table_cell_definition.attribute_index = elems[2].as.int8.value;
			return OSP_OK;
		}
		case 4: /* scaler_unit */
			return osp_ic_read_scaler_unit(value, &i->scaler_unit);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t rt_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_register_table_class(), inst, buf, rt_attrs, 4);
}

static osp_err_t rt_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_register_table_class(), inst, buf, rt_attrs, 4);
}

static const osp_ic_class_t ic_register_table = {
    .name = "Register Table",
    .class_id = 61,
    .version = 0,
    .get_attr = rt_get,
    .set_attr = rt_set,
    .invoke = NULL,
    .serialize = rt_serialize,
    .deserialize = rt_deserialize,
    .instance_size = sizeof(osp_ic_register_table_t),
};

const osp_ic_class_t *osp_ic_register_table_class(void) {
	return &ic_register_table;
}

void osp_ic_register_table_init(osp_ic_register_table_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
