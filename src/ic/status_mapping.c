#include "status_mapping.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t sm_attrs[] = {1, 2};

static osp_err_t sm_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_status_mapping_t *i = (const osp_ic_status_mapping_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
		case 2: {
			OSP_TLS osp_value_t rows[OSP_MAX_STATUS_MAPPINGS];
			OSP_TLS osp_value_t fields[OSP_MAX_STATUS_MAPPINGS][2];
			uint8_t n = i->entry_count;
			if (n > OSP_MAX_STATUS_MAPPINGS) {
				n = OSP_MAX_STATUS_MAPPINGS;
			}
			for (uint8_t idx = 0; idx < n; idx++) {
				fields[idx][0] = osp_val_u8(i->entries[idx].status_flag_id);
				fields[idx][1].tag = OSP_TAG_OCTETSTRING;
				fields[idx][1].as.octetstring.len = 6;
				memcpy(fields[idx][1].as.octetstring.data, i->entries[idx].status_reference, 6);
				rows[idx].tag = OSP_TAG_STRUCTURE;
				rows[idx].as.structure.elements.items = fields[idx];
				rows[idx].as.structure.elements.count = 2;
				rows[idx].as.structure.elements.capacity = 2;
			}
			result->tag = OSP_TAG_ARRAY;
			result->as.array.elements.items = rows;
			result->as.array.elements.count = n;
			result->as.array.elements.capacity = OSP_MAX_STATUS_MAPPINGS;
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sm_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_status_mapping_t *i = (osp_ic_status_mapping_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2: {
			/* Store status_mappings from incoming array */
			if (value->tag != OSP_TAG_ARRAY) {
				return OSP_ERR_INVALID;
			}
			i->entry_count = value->as.array.elements.count;
			if (i->entry_count > OSP_MAX_STATUS_MAPPINGS) {
				i->entry_count = OSP_MAX_STATUS_MAPPINGS;
			}
			for (uint8_t idx = 0; idx < i->entry_count; idx++) {
				const osp_value_t *item = &value->as.array.elements.items[idx];
				if (item->tag == OSP_TAG_STRUCTURE && item->as.structure.elements.count >= 2) {
					i->entries[idx].status_flag_id = item->as.structure.elements.items[0].as.uint8.value;
					if (item->as.structure.elements.items[1].tag == OSP_TAG_OCTETSTRING) {
						uint32_t len = item->as.structure.elements.items[1].as.octetstring.len;
						if (len > 6) len = 6;
						memcpy(i->entries[idx].status_reference, item->as.structure.elements.items[1].as.octetstring.data, len);
					}
				}
			}
			return OSP_OK;
		}
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t sm_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_status_mapping_class(), inst, buf, sm_attrs, 2);
}

static osp_err_t sm_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_status_mapping_class(), inst, buf, sm_attrs, 2);
}

static const osp_ic_class_t ic_status_mapping = {
    .name = "Status Mapping",
    .class_id = 63,
    .version = 0,
    .get_attr = sm_get,
    .set_attr = sm_set,
    .invoke = NULL,
    .serialize = sm_serialize,
    .deserialize = sm_deserialize,
    .instance_size = sizeof(osp_ic_status_mapping_t),
};

const osp_ic_class_t *osp_ic_status_mapping_class(void) {
	return &ic_status_mapping;
}

void osp_ic_status_mapping_init(osp_ic_status_mapping_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
