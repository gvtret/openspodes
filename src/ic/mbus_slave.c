#include "mbus_slave.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ms_attrs[] = {1, 2, 3, 4, 5, 6, 7};

static osp_err_t ms_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_mbus_slave_t *i = (const osp_ic_mbus_slave_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
		case 2:
			*result = osp_val_u16(i->physical_address);
			return OSP_OK;
		case 3:
			*result = osp_val_u16(i->logical_address);
			return OSP_OK;
		case 4:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = i->id_number_len;
			memcpy(result->as.octetstring.data, i->id_number, i->id_number_len);
			return OSP_OK;
		case 5:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = i->manufacturer_len;
			memcpy(result->as.octetstring.data, i->manufacturer, i->manufacturer_len);
			return OSP_OK;
		case 6:
			*result = osp_val_u8(i->version);
			return OSP_OK;
		case 7:
			*result = osp_val_u8(i->medium);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ms_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_mbus_slave_t *i = (osp_ic_mbus_slave_t *)inst;
	if (!value) return OSP_ERR_INVALID;
	switch (attr_id) {
		case 2: i->physical_address = osp_get_u16(value); return OSP_OK;
		case 3: i->logical_address = osp_get_u16(value); return OSP_OK;
		case 6: i->version = osp_get_u8(value); return OSP_OK;
		case 7: i->medium = osp_get_u8(value); return OSP_OK;
		default: return OSP_OK; /* accept but don't store */
	}
}

static osp_err_t ms_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_mbus_slave_class(), inst, buf, ms_attrs, 7);
}

static osp_err_t ms_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_mbus_slave_class(), inst, buf, ms_attrs, 7);
}

static const osp_ic_class_t ic_mbus_slave = {
    .name = "MBus Slave Port Setup",
    .class_id = 76,
    .version = 0,
    .get_attr = ms_get,
    .set_attr = ms_set,
    .invoke = NULL,
    .serialize = ms_serialize,
    .deserialize = ms_deserialize,
    .instance_size = sizeof(osp_ic_mbus_slave_t),
};

const osp_ic_class_t *osp_ic_mbus_slave_class(void) {
	return &ic_mbus_slave;
}

void osp_ic_mbus_slave_init(osp_ic_mbus_slave_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
