#include "iec_hdlc_setup.h"
#include <string.h>

static osp_err_t hdlc_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_iec_hdlc_setup_t *h = (const osp_ic_iec_hdlc_setup_t *)inst;
	switch (attr_id) {
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

static const osp_ic_class_t ic_hdlc = {
    .name = "IEC HDLC Setup",
    .class_id = 23,
    .version = 0,
    .get_attr = hdlc_get,
    .set_attr = NULL,
    .invoke = NULL,
    .instance_size = sizeof(osp_ic_iec_hdlc_setup_t)
};

const osp_ic_class_t *osp_ic_iec_hdlc_setup_class(void) {
	return &ic_hdlc;
}

void osp_ic_iec_hdlc_setup_init(osp_ic_iec_hdlc_setup_t *h, osp_obis_t ln) {
	memset(h, 0, sizeof(*h));
	h->logical_name = ln;
}
