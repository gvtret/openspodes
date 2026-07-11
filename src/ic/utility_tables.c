#include "utility_tables.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t ut_attrs[] = {1, 2, 3, 4};

static osp_err_t ut_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_utility_tables_t *i = (const osp_ic_utility_tables_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
		case 2:
			*result = osp_val_u16(i->table_id);
			return OSP_OK;
		case 3:
			*result = osp_val_u32(i->length);
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = i->buffer_len;
			if (result->as.octetstring.len > OSP_MAX_OCTET_LEN) {
				result->as.octetstring.len = OSP_MAX_OCTET_LEN;
			}
			memcpy(result->as.octetstring.data, i->buffer, result->as.octetstring.len);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ut_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (!value) return OSP_ERR_INVALID;
	/* All attrs accept but don't store (read-only stubs) */
	return OSP_OK;
}

static osp_err_t ut_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_utility_tables_class(), inst, buf, ut_attrs, 4);
}

static osp_err_t ut_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_utility_tables_class(), inst, buf, ut_attrs, 4);
}

static const osp_ic_class_t ic_utility_tables = {
    .name = "Utility Tables",
    .class_id = 26,
    .version = 0,
    .get_attr = ut_get,
    .set_attr = ut_set,
    .invoke = NULL,
    .serialize = ut_serialize,
    .deserialize = ut_deserialize,
    .instance_size = sizeof(osp_ic_utility_tables_t),
};

const osp_ic_class_t *osp_ic_utility_tables_class(void) {
	return &ic_utility_tables;
}

void osp_ic_utility_tables_init(osp_ic_utility_tables_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
