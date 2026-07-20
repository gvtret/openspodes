#include "ipv6_setup.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t ipv6_attrs[] = {1, 2, 3, 7, 8, 9};

static osp_err_t ipv6_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_ipv6_setup_t *s = (const osp_ic_ipv6_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = 6;
			memcpy(result->as.octetstring.data, &s->dl_reference, 6);
			return OSP_OK;
		case 3:
			*result = osp_val_enum(s->address_config_mode);
			return OSP_OK;
		case 7:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = s->primary_dns_len;
			memcpy(result->as.octetstring.data, s->primary_dns, s->primary_dns_len);
			return OSP_OK;
		case 8:
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = s->secondary_dns_len;
			memcpy(result->as.octetstring.data, s->secondary_dns, s->secondary_dns_len);
			return OSP_OK;
		case 9:
			*result = osp_val_u8(s->traffic_class);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ipv6_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_ipv6_setup_t *s = (osp_ic_ipv6_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len != 6) {
				return OSP_ERR_INVALID;
			}
			memcpy(&s->dl_reference, value->as.octetstring.data, 6);
			return OSP_OK;
		case 3:
			s->address_config_mode = osp_get_enum(value);
			return OSP_OK;
		case 7:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len > 16) {
				return OSP_ERR_INVALID;
			}
			s->primary_dns_len = (uint8_t)value->as.octetstring.len;
			memcpy(s->primary_dns, value->as.octetstring.data, s->primary_dns_len);
			return OSP_OK;
		case 8:
			if (value->tag != OSP_TAG_OCTETSTRING || value->as.octetstring.len > 16) {
				return OSP_ERR_INVALID;
			}
			s->secondary_dns_len = (uint8_t)value->as.octetstring.len;
			memcpy(s->secondary_dns, value->as.octetstring.data, s->secondary_dns_len);
			return OSP_OK;
		case 9:
			s->traffic_class = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ipv6_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_ipv6_setup_class(), inst, buf, ipv6_attrs, 6);
}

static osp_err_t ipv6_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_ipv6_setup_class(), inst, buf, ipv6_attrs, 6);
}

static const osp_ic_class_t ic_ipv6_setup = {
    .name = "IPv6 Setup",
    .class_id = 48,
    .version = 0,
    .get_attr = ipv6_get,
    .set_attr = ipv6_set,
    .invoke = NULL,
    .serialize = ipv6_serialize,
    .deserialize = ipv6_deserialize,
    .instance_size = sizeof(osp_ic_ipv6_setup_t),
};

const osp_ic_class_t *osp_ic_ipv6_setup_class(void) {
	return &ic_ipv6_setup;
}

void osp_ic_ipv6_setup_init(osp_ic_ipv6_setup_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
