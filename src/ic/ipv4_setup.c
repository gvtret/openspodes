#include "ipv4_setup.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t ipv4_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static osp_err_t ipv4_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_ipv4_setup_t *s = (const osp_ic_ipv4_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &s->logical_name);
		case 2: {
			result->tag = OSP_TAG_OCTETSTRING;
			result->as.octetstring.len = 6;
			memcpy(result->as.octetstring.data, &s->dl_reference, 6);
			return OSP_OK;
		}
		case 3:
			*result = osp_val_u32(s->ip_address);
			return OSP_OK;
		case 4: {
			static osp_value_t items[OSP_MAX_IP_MULTICAST];
			osp_value_t v = {0};
			uint8_t n = s->multicast_count;
			if (n > OSP_MAX_IP_MULTICAST) {
				n = OSP_MAX_IP_MULTICAST;
			}
			for (uint8_t i = 0; i < n; i++) {
				items[i] = osp_val_u32(s->multicast_ip[i]);
			}
			v.tag = OSP_TAG_ARRAY;
			v.as.array.elements.items = items;
			v.as.array.elements.count = n;
			v.as.array.elements.capacity = OSP_MAX_IP_MULTICAST;
			*result = v;
			return OSP_OK;
		}
		case 5:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 6:
			*result = osp_val_u32(s->subnet_mask);
			return OSP_OK;
		case 7:
			*result = osp_val_u32(s->gateway_ip);
			return OSP_OK;
		case 8:
			*result = osp_val_bool(s->use_dhcp);
			return OSP_OK;
		case 9:
			*result = osp_val_u32(s->primary_dns);
			return OSP_OK;
		case 10:
			*result = osp_val_u32(s->secondary_dns);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ipv4_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_ipv4_setup_t *s = (osp_ic_ipv4_setup_t *)inst;
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
			s->ip_address = osp_get_u32(value);
			return OSP_OK;
		case 4:
		case 5:
			return OSP_OK;
		case 6:
			s->subnet_mask = osp_get_u32(value);
			return OSP_OK;
		case 7:
			s->gateway_ip = osp_get_u32(value);
			return OSP_OK;
		case 8:
			s->use_dhcp = value->as.boolean.value;
			return OSP_OK;
		case 9:
			s->primary_dns = osp_get_u32(value);
			return OSP_OK;
		case 10:
			s->secondary_dns = osp_get_u32(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t ipv4_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_ipv4_setup_class(), inst, buf, ipv4_attrs, 10);
}

static osp_err_t ipv4_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_ipv4_setup_class(), inst, buf, ipv4_attrs, 10);
}

static const osp_ic_class_t ic_ipv4 = {
    .name = "IPv4 Setup",
    .class_id = 42,
    .version = 0,
    .get_attr = ipv4_get,
    .set_attr = ipv4_set,
    .invoke = NULL,
    .serialize = ipv4_serialize,
    .deserialize = ipv4_deserialize,
    .instance_size = sizeof(osp_ic_ipv4_setup_t),
};

const osp_ic_class_t *osp_ic_ipv4_setup_class(void) {
	return &ic_ipv4;
}

void osp_ic_ipv4_setup_init(osp_ic_ipv4_setup_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
