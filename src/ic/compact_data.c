#include "compact_data.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include "../server/dispatcher.h"
#include <string.h>

static const uint8_t cd_attrs[] = {1};

static osp_value_t cd_val_capture_objects(const osp_capture_object_list_t *list) {
	OSP_TLS osp_value_t items[OSP_MAX_CAPTURE_OBJECTS];
	uint8_t n = list ? list->count : 0;
	for (uint8_t i = 0; i < n; i++) {
		items[i] = osp_ic_val_capture_object(&list->items[i]);
	}
	osp_value_t v = osp_ic_val_empty_array();
	if (n > 0) {
		v.as.array.elements.items = items;
		v.as.array.elements.count = n;
		v.as.array.elements.capacity = n;
	}
	return v;
}

static osp_err_t cd_read_capture_objects(const osp_value_t *value, osp_capture_object_list_t *list) {
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

static osp_value_t cd_val_octets(const uint8_t *data, uint32_t len) {
	osp_value_t v;
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = len;
	if (len > 0 && data) {
		memcpy(v.as.octetstring.data, data, len);
	}
	return v;
}

static osp_err_t cd_read_octets(const osp_value_t *value, uint8_t *out, uint32_t out_max, uint32_t *out_len) {
	if (!value || !out || !out_len || value->tag != OSP_TAG_OCTETSTRING) {
		return OSP_ERR_INVALID;
	}
	if (value->as.octetstring.len > out_max) {
		return OSP_ERR_NOMEM;
	}
	*out_len = value->as.octetstring.len;
	memcpy(out, value->as.octetstring.data, *out_len);
	return OSP_OK;
}

static osp_err_t cd_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_compact_data_t *c = (const osp_ic_compact_data_t *)inst;
	if (!result) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &c->logical_name);
		case 2:
			*result = cd_val_octets(c->compact_buffer, c->compact_buffer_len);
			return OSP_OK;
		case 3:
			*result = cd_val_capture_objects(&c->capture_objects);
			return OSP_OK;
		case 4:
			*result = osp_val_u8(c->template_id);
			return OSP_OK;
		case 5:
			*result = cd_val_octets(c->template_description, c->template_description_len);
			return OSP_OK;
		case 6:
			*result = osp_val_enum((uint8_t)c->capture_method);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t cd_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_compact_data_t *c = (osp_ic_compact_data_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			return cd_read_octets(value, c->compact_buffer, sizeof(c->compact_buffer), &c->compact_buffer_len);
		case 3:
			return cd_read_capture_objects(value, &c->capture_objects);
		case 4:
			c->template_id = osp_get_u8(value);
			return OSP_OK;
		case 5:
			return cd_read_octets(value, c->template_description, sizeof(c->template_description), &c->template_description_len);
		case 6:
			c->capture_method = (osp_compact_capture_method_t)osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t cd_encode_rows(const osp_value_t *rows, uint8_t row_count, uint8_t *out, uint32_t out_max, uint32_t *out_len) {
	if (!rows || row_count == 0 || !out || !out_len) {
		return OSP_ERR_INVALID;
	}

	osp_value_t arr;
	arr.tag = OSP_TAG_ARRAY;
	arr.as.array.elements.items = (osp_value_t *)rows;
	arr.as.array.elements.count = row_count;
	arr.as.array.elements.capacity = row_count;

	osp_buf_t buf;
	osp_buf_init(&buf, out, out_max);
	osp_err_t r = osp_value_write_compact_array(&buf, &arr);
	if (r != OSP_OK) {
		return r;
	}
	*out_len = buf.wr;
	return OSP_OK;
}

static osp_err_t cd_capture_from_dispatcher(osp_ic_compact_data_t *c) {
	if (c->dispatcher && c->capture_objects.count > 0) {
		osp_value_t fields[OSP_MAX_CAPTURE_OBJECTS];
		for (uint8_t i = 0; i < c->capture_objects.count; i++) {
			const osp_capture_object_t *co = &c->capture_objects.items[i];
			osp_err_t r = osp_dispatcher_get(c->dispatcher, co->class_id, &co->logical_name, (uint8_t)co->attribute_index, &fields[i]);
			if (r != OSP_OK) {
				return r;
			}
		}
		osp_value_t row;
		row.tag = OSP_TAG_STRUCTURE;
		row.as.structure.elements.items = fields;
		row.as.structure.elements.count = c->capture_objects.count;
		row.as.structure.elements.capacity = c->capture_objects.count;
		osp_value_t rows[1] = {row};
		return cd_encode_rows(rows, 1, c->compact_buffer, sizeof(c->compact_buffer), &c->compact_buffer_len);
	}
	if (c->capture_value_count > 0) {
		return cd_encode_rows(c->capture_values, c->capture_value_count, c->compact_buffer, sizeof(c->compact_buffer), &c->compact_buffer_len);
	}
	return OSP_ERR_INVALID;
}

static osp_err_t cd_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_compact_data_t *c = (osp_ic_compact_data_t *)inst;
	(void)param;
	if (result) {
		*result = osp_val_null();
	}
	switch (method_id) {
		case 1:
			c->compact_buffer_len = 0;
			c->capture_value_count = 0;
			return OSP_OK;
		case 2:
			return cd_capture_from_dispatcher(c);
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t cd_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_compact_data_class(), inst, buf, cd_attrs, 1);
}

static osp_err_t cd_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_compact_data_class(), inst, buf, cd_attrs, 1);
}

static const osp_ic_class_t ic_compact_data = {
    .name = "Compact Data",
    .class_id = 62,
    .version = 0,
    .get_attr = cd_get,
    .set_attr = cd_set,
    .invoke = cd_invoke,
    .serialize = cd_serialize,
    .deserialize = cd_deserialize,
    .instance_size = sizeof(osp_ic_compact_data_t),
};

const osp_ic_class_t *osp_ic_compact_data_class(void) {
	return &ic_compact_data;
}

void osp_ic_compact_data_init(osp_ic_compact_data_t *i, osp_obis_t ln) {
	if (!i) {
		return;
	}
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}

void osp_ic_compact_data_bind_dispatcher(osp_ic_compact_data_t *i, osp_dispatcher_t *disp) {
	if (i) {
		i->dispatcher = disp;
	}
}

void osp_ic_compact_data_set_capture_values(osp_ic_compact_data_t *i, const osp_value_t *values, uint8_t count) {
	if (!i || !values || count > OSP_MAX_CAPTURE_OBJECTS) {
		return;
	}
	for (uint8_t n = 0; n < count; n++) {
		i->capture_values[n] = values[n];
	}
	i->capture_value_count = count;
}
