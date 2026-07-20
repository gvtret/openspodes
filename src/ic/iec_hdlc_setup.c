#include "iec_hdlc_setup.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t hdlc_attrs[] = {1, 2, 3, 5, 6, 7, 8, 9};

static osp_err_t hdlc_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_iec_hdlc_setup_t *h = (const osp_ic_iec_hdlc_setup_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &h->logical_name);
		case 2:
			*result = osp_val_u8(h->communication_port);
			return OSP_OK;
		case 3:
			*result = osp_val_u8(h->communication_speed);
			return OSP_OK;
		case 5:
			*result = osp_val_u16(h->logical_station_address);
			return OSP_OK;
		case 6:
			*result = osp_val_u8(h->window_size_tx);
			return OSP_OK;
		case 7:
			*result = osp_val_u8(h->window_size_rx);
			return OSP_OK;
		case 8:
			*result = osp_val_u16(h->max_info_field_length_tx);
			return OSP_OK;
		case 9:
			*result = osp_val_u16(h->max_info_field_length_rx);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t hdlc_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_iec_hdlc_setup_t *h = (osp_ic_iec_hdlc_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			h->communication_port = osp_get_u8(value);
			return OSP_OK;
		case 3:
			h->communication_speed = osp_get_u8(value);
			return OSP_OK;
		case 5:
			h->logical_station_address = osp_get_u16(value);
			return OSP_OK;
		case 6:
			h->window_size_tx = osp_get_u8(value);
			return OSP_OK;
		case 7:
			h->window_size_rx = osp_get_u8(value);
			return OSP_OK;
		case 8:
			h->max_info_field_length_tx = osp_get_u16(value);
			return OSP_OK;
		case 9:
			h->max_info_field_length_rx = osp_get_u16(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t hdlc_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_iec_hdlc_setup_class(), inst, buf, hdlc_attrs, 8);
}

static osp_err_t hdlc_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_iec_hdlc_setup_class(), inst, buf, hdlc_attrs, 8);
}

static const osp_ic_class_t ic_hdlc = {
    .name = "IEC HDLC Setup",
    .class_id = 23,
    .version = 0,
    .get_attr = hdlc_get,
    .set_attr = hdlc_set,
    .invoke = NULL,
    .serialize = hdlc_serialize,
    .deserialize = hdlc_deserialize,
    .instance_size = sizeof(osp_ic_iec_hdlc_setup_t),
};

const osp_ic_class_t *osp_ic_iec_hdlc_setup_class(void) {
	return &ic_hdlc;
}

void osp_ic_iec_hdlc_setup_init(osp_ic_iec_hdlc_setup_t *h, osp_obis_t ln) {
	memset(h, 0, sizeof(*h));
	h->logical_name = ln;
}
