#ifndef OSP_IC_IEC_HDLC_SETUP_H
#define OSP_IC_IEC_HDLC_SETUP_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t communication_port;
	uint8_t communication_speed;
	uint16_t logical_station_address;
	uint8_t mac_address[6];
	uint16_t max_info_field_length_tx;
	uint16_t max_info_field_length_rx;
	uint8_t window_size_tx;
	uint8_t window_size_rx;
} osp_ic_iec_hdlc_setup_t;

void osp_ic_iec_hdlc_setup_init(osp_ic_iec_hdlc_setup_t *h, osp_obis_t ln);
const osp_ic_class_t *osp_ic_iec_hdlc_setup_class(void);
#endif
