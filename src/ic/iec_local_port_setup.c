#include "iec_local_port_setup.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t lp_attrs[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

static osp_value_t lp_octet(const uint8_t *data, uint8_t len) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = len;
	memcpy(v.as.octetstring.data, data, len);
	return v;
}

static osp_err_t lp_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_iec_local_port_setup_t *p = (const osp_ic_iec_local_port_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &p->logical_name);
		case 2:
			*result = osp_val_enum(p->default_mode);
			return OSP_OK;
		case 3:
			*result = osp_val_enum(p->default_baud_rate);
			return OSP_OK;
		case 4:
			*result = osp_val_enum(p->proposed_baud_rate);
			return OSP_OK;
		case 5:
			*result = osp_val_u8(p->response_time);
			return OSP_OK;
		case 6:
			*result = lp_octet(p->device_address, p->device_address_len);
			return OSP_OK;
		case 7:
			*result = lp_octet(p->pass_p1, p->pass_p1_len);
			return OSP_OK;
		case 8:
			*result = lp_octet(p->pass_p2, p->pass_p2_len);
			return OSP_OK;
		case 9:
			*result = osp_val_u8(p->pass_w5);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lp_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_iec_local_port_setup_t *p = (osp_ic_iec_local_port_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			p->default_mode = osp_get_enum(value);
			return OSP_OK;
		case 3:
			p->default_baud_rate = osp_get_enum(value);
			return OSP_OK;
		case 4:
			p->proposed_baud_rate = osp_get_enum(value);
			return OSP_OK;
		case 5:
			p->response_time = osp_get_u8(value);
			return OSP_OK;
		case 6:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			p->device_address_len = (uint8_t)value->as.octetstring.len;
			if (p->device_address_len > sizeof(p->device_address)) {
				p->device_address_len = sizeof(p->device_address);
			}
			memcpy(p->device_address, value->as.octetstring.data, p->device_address_len);
			return OSP_OK;
		case 7:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			p->pass_p1_len = (uint8_t)value->as.octetstring.len;
			if (p->pass_p1_len > sizeof(p->pass_p1)) {
				p->pass_p1_len = sizeof(p->pass_p1);
			}
			memcpy(p->pass_p1, value->as.octetstring.data, p->pass_p1_len);
			return OSP_OK;
		case 8:
			if (value->tag != OSP_TAG_OCTETSTRING) {
				return OSP_ERR_INVALID;
			}
			p->pass_p2_len = (uint8_t)value->as.octetstring.len;
			if (p->pass_p2_len > sizeof(p->pass_p2)) {
				p->pass_p2_len = sizeof(p->pass_p2);
			}
			memcpy(p->pass_p2, value->as.octetstring.data, p->pass_p2_len);
			return OSP_OK;
		case 9:
			p->pass_w5 = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t lp_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_iec_local_port_setup_class(), inst, buf, lp_attrs, 9);
}

static osp_err_t lp_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_iec_local_port_setup_class(), inst, buf, lp_attrs, 9);
}

static const osp_ic_class_t ic_lp = {
    .name = "IEC Local Port Setup",
    .class_id = 19,
    .version = 0,
    .get_attr = lp_get,
    .set_attr = lp_set,
    .invoke = NULL,
    .serialize = lp_serialize,
    .deserialize = lp_deserialize,
    .instance_size = sizeof(osp_ic_iec_local_port_setup_t),
};

const osp_ic_class_t *osp_ic_iec_local_port_setup_class(void) {
	return &ic_lp;
}

void osp_ic_iec_local_port_setup_init(osp_ic_iec_local_port_setup_t *p, osp_obis_t ln) {
	memset(p, 0, sizeof(*p));
	p->logical_name = ln;
}
