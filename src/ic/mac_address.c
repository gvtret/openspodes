#include "mac_address.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t mac_attrs[] = {1, 2};

static osp_err_t mac_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_mac_address_t *m = (const osp_ic_mac_address_t *)inst;
	if (attr_id == 1) {
		return osp_ic_get_logical_name(result, &m->logical_name);
	}
	if (attr_id == 2) {
		result->tag = OSP_TAG_OCTETSTRING;
		result->as.octetstring.len = m->mac_address_len;
		memcpy(result->as.octetstring.data, m->mac_address, m->mac_address_len);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t mac_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_mac_address_t *m = (osp_ic_mac_address_t *)inst;
	if (attr_id == 2 && value && value->tag == OSP_TAG_OCTETSTRING) {
		m->mac_address_len = (uint8_t)value->as.octetstring.len;
		if (m->mac_address_len > sizeof(m->mac_address)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(m->mac_address, value->as.octetstring.data, m->mac_address_len);
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

static osp_err_t mac_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_mac_address_class(), inst, buf, mac_attrs, 2);
}

static osp_err_t mac_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_mac_address_class(), inst, buf, mac_attrs, 2);
}

static const osp_ic_class_t ic_mac = {
    .name = "MAC Address Setup",
    .class_id = 43,
    .version = 0,
    .get_attr = mac_get,
    .set_attr = mac_set,
    .invoke = NULL,
    .serialize = mac_serialize,
    .deserialize = mac_deserialize,
    .instance_size = sizeof(osp_ic_mac_address_t),
};

const osp_ic_class_t *osp_ic_mac_address_class(void) {
	return &ic_mac;
}

void osp_ic_mac_address_init(osp_ic_mac_address_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
