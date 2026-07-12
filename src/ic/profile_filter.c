#include "profile_filter.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t pf_attrs[] = {1, 2, 3, 4};

static osp_err_t pf_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_profile_filter_t *f = (const osp_ic_profile_filter_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &f->logical_name);
		case 2:
			*result = osp_val_bool(f->filter_enable);
			return OSP_OK;
		case 3:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = f->filter_name_len;
			memcpy(result->as.octetstring.data, f->filter_name, f->filter_name_len);
			return OSP_OK;
		case 4: {
			/* Return capture_objects as array of structures */
			osp_value_list_t *list = &result->as.array.elements;
			result->tag = OSP_TAG_ARRAY;
			list->count = f->capture_objects.count;
			for (uint8_t i = 0; i < f->capture_objects.count && i < OSP_MAX_CAPTURE_OBJECTS; i++) {
				osp_value_t *item = &list->items[i];
				item->tag = OSP_TAG_STRUCTURE;
				osp_value_list_t *elems = &item->as.structure.elements;
				elems->count = 3;
				elems->items[0] = osp_val_u16(f->capture_objects.items[i].class_id);
				elems->items[1].tag = OSP_TAG_OCTETSTRING;
				elems->items[1].as.octetstring.len = 6;
				memcpy(elems->items[1].as.octetstring.data, &f->capture_objects.items[i].logical_name, 6);
				elems->items[2] = osp_val_u8(f->capture_objects.items[i].attribute_index);
			}
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pf_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_profile_filter_t *f = (osp_ic_profile_filter_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			f->filter_enable = osp_get_bool(value);
			return OSP_OK;
		case 3:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			f->filter_name_len = (uint8_t)value->as.octetstring.len;
			if (f->filter_name_len > OSP_MAX_FILTER_NAME_LEN) {
				f->filter_name_len = OSP_MAX_FILTER_NAME_LEN;
			}
			memcpy(f->filter_name, value->as.octetstring.data, f->filter_name_len);
			return OSP_OK;
		case 4:
			/* Store capture_objects from incoming array */
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			f->capture_objects.count = value->as.array.elements.count;
			if (f->capture_objects.count > OSP_MAX_CAPTURE_OBJECTS) {
				f->capture_objects.count = OSP_MAX_CAPTURE_OBJECTS;
			}
			for (uint8_t i = 0; i < f->capture_objects.count; i++) {
				const osp_value_t *item = &value->as.array.elements.items[i];
				if (item->tag == OSP_TAG_STRUCTURE && item->as.structure.elements.count >= 3) {
					osp_capture_object_t *co = &f->capture_objects.items[i];
					co->class_id = item->as.structure.elements.items[0].as.uint16.value;
					if (item->as.structure.elements.items[1].tag == OSP_TAG_OCTETSTRING) {
						memcpy(&co->logical_name, item->as.structure.elements.items[1].as.octetstring.data, sizeof(osp_obis_t));
					}
					co->attribute_index = item->as.structure.elements.items[2].as.uint8.value;
				}
			}
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t pf_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_profile_filter_class(), inst, buf, pf_attrs, 4);
}

static osp_err_t pf_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_profile_filter_class(), inst, buf, pf_attrs, 4);
}

static const osp_ic_class_t ic_pf = {
    .name = "Profile Filter",
    .class_id = 31,
    .version = 0,
    .get_attr = pf_get,
    .set_attr = pf_set,
    .invoke = NULL,
    .serialize = pf_serialize,
    .deserialize = pf_deserialize,
    .instance_size = sizeof(osp_ic_profile_filter_t),
};

const osp_ic_class_t *osp_ic_profile_filter_class(void) {
	return &ic_pf;
}

void osp_ic_profile_filter_init(osp_ic_profile_filter_t *f, osp_obis_t ln) {
	memset(f, 0, sizeof(*f));
	f->logical_name = ln;
}
