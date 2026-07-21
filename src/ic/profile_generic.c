#include "profile_generic.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t pg_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8};

static osp_value_t pg_val_buffer(const osp_profile_buffer_t *buf) {
	OSP_TLS osp_value_t rows[OSP_MAX_BUFFER_ROWS];
	OSP_TLS osp_value_t cells[OSP_MAX_BUFFER_ROWS][OSP_MAX_CAPTURE_OBJECTS];
	osp_value_t v = {0};
	if (!buf || buf->row_count == 0) {
		return osp_ic_val_empty_array();
	}
	for (uint8_t i = 0; i < buf->row_count && i < OSP_MAX_BUFFER_ROWS; i++) {
		const osp_profile_row_t *row = &buf->rows[i];
		rows[i].tag = OSP_TAG_STRUCTURE;
		rows[i].as.structure.elements.items = cells[i];
		rows[i].as.structure.elements.count = row->cell_count;
		rows[i].as.structure.elements.capacity = row->cell_count;
		for (uint8_t j = 0; j < row->cell_count && j < OSP_MAX_CAPTURE_OBJECTS; j++) {
			cells[i][j] = row->cells[j];
		}
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = rows;
	v.as.array.elements.count = buf->row_count;
	v.as.array.elements.capacity = buf->row_count;
	return v;
}

static osp_value_t pg_val_capture_objects(const osp_capture_object_list_t *list) {
	/* Per-item field storage: osp_ic_val_capture_object uses TLS and cannot be
	 * called repeatedly into the same array without overwriting prior elements. */
	OSP_TLS osp_value_t items[OSP_MAX_CAPTURE_OBJECTS];
	OSP_TLS osp_value_t fields[OSP_MAX_CAPTURE_OBJECTS][4];
	uint8_t n = list ? list->count : 0;
	if (n > OSP_MAX_CAPTURE_OBJECTS) {
		n = OSP_MAX_CAPTURE_OBJECTS;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_capture_object_t *co = &list->items[i];
		fields[i][0] = osp_val_u16(co->class_id);
		fields[i][1].tag = OSP_TAG_OCTETSTRING;
		fields[i][1].as.octetstring.len = 6;
		memcpy(fields[i][1].as.octetstring.data, &co->logical_name, 6);
		fields[i][2] = osp_val_i8(co->attribute_index);
		fields[i][3] = osp_val_u16((uint16_t)co->data_index);
		items[i].tag = OSP_TAG_STRUCTURE;
		items[i].as.structure.elements.items = fields[i];
		items[i].as.structure.elements.count = 4;
		items[i].as.structure.elements.capacity = 4;
	}
	osp_value_t v = osp_ic_val_empty_array();
	if (n > 0) {
		v.as.array.elements.items = items;
		v.as.array.elements.count = n;
		v.as.array.elements.capacity = n;
	}
	return v;
}

static osp_err_t pg_read_capture_objects(const osp_value_t *value, osp_capture_object_list_t *list) {
	if (!value || !list || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	list->count = 0;
	for (uint8_t i = 0; i < value->as.array.elements.count && i < OSP_MAX_CAPTURE_OBJECTS; i++) {
		if (osp_ic_read_capture_object(&value->as.array.elements.items[i], &list->items[list->count]) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
		list->count++;
	}
	return OSP_OK;
}

static osp_err_t pg_read_buffer(const osp_value_t *value, osp_profile_buffer_t *buf) {
	if (!value || !buf) {
		return OSP_ERR_INVALID;
	}
	buf->row_count = 0;
	if (value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	for (uint8_t i = 0; i < value->as.array.elements.count && i < OSP_MAX_BUFFER_ROWS; i++) {
		const osp_value_t *row = &value->as.array.elements.items[i];
		if (row->tag != OSP_TAG_STRUCTURE) {
			return OSP_ERR_INVALID;
		}
		osp_profile_row_t *dst = &buf->rows[i];
		dst->cell_count = 0;
		for (uint8_t j = 0; j < row->as.structure.elements.count && j < OSP_MAX_CAPTURE_OBJECTS; j++) {
			dst->cells[j] = row->as.structure.elements.items[j];
			dst->cell_count++;
		}
		buf->row_count++;
	}
	return OSP_OK;
}

static osp_err_t pg_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_profile_generic_t *p = (const osp_ic_profile_generic_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &p->logical_name);
		case 2:
			*result = pg_val_buffer(&p->buffer);
			return OSP_OK;
		case 3:
			*result = pg_val_capture_objects(&p->capture_objects);
			return OSP_OK;
		case 4:
			*result = osp_val_u32(p->capture_period);
			return OSP_OK;
		case 5:
			*result = osp_val_u8((uint8_t)p->sort_method);
			return OSP_OK;
		case 6:
			*result = osp_ic_val_capture_object(&p->sort_object);
			return OSP_OK;
		case 7:
			*result = osp_val_u32(p->entries_in_use);
			return OSP_OK;
		case 8:
			*result = osp_val_u32(p->profile_entries);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pg_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_profile_generic_t *p = (osp_ic_profile_generic_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return pg_read_buffer(value, &p->buffer);
		case 3:
			return pg_read_capture_objects(value, &p->capture_objects);
		case 4:
			p->capture_period = osp_get_u32(value);
			return OSP_OK;
		case 5:
			p->sort_method = (osp_sort_method_t)osp_get_u8(value);
			return OSP_OK;
		case 6:
			return osp_ic_read_capture_object(value, &p->sort_object);
		case 7:
			p->entries_in_use = osp_get_u32(value);
			return OSP_OK;
		case 8:
			p->profile_entries = osp_get_u32(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pg_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_profile_generic_t *p = (osp_ic_profile_generic_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 1) {
		p->buffer.row_count = 0;
		p->entries_in_use = 0;
		return OSP_OK;
	}
	if (method_id == 2) {
		return OSP_OK;
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t pg_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_profile_generic_class(), inst, buf, pg_attrs, 8);
}

static osp_err_t pg_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_profile_generic_class(), inst, buf, pg_attrs, 8);
}

static const osp_ic_class_t ic_pg = {
    .name = "Profile Generic",
    .class_id = 7,
    .version = 1,
    .get_attr = pg_get,
    .set_attr = pg_set,
    .invoke = pg_invoke,
    .serialize = pg_serialize,
    .deserialize = pg_deserialize,
    .instance_size = sizeof(osp_ic_profile_generic_t),
};

const osp_ic_class_t *osp_ic_profile_generic_class(void) {
	return &ic_pg;
}

void osp_ic_profile_generic_init(osp_ic_profile_generic_t *p, osp_obis_t ln) {
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}
