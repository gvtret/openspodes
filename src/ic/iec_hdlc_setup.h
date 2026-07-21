#ifndef OSP_IC_IEC_HDLC_SETUP_H
#define OSP_IC_IEC_HDLC_SETUP_H
#include "../openspodes.h"

/* IEC HDLC setup (class_id = 23). Blue Book / IEC 62056-6-2 attributes 2..9. */
typedef struct {
	osp_obis_t logical_name;
	uint8_t comm_speed; /* enum */
	uint8_t window_size_transmit;
	uint8_t window_size_receive;
	uint16_t max_info_field_length_transmit;
	uint16_t max_info_field_length_receive;
	uint16_t inter_octet_time_out;
	uint16_t inactivity_time_out;
	uint16_t device_address;
} osp_ic_iec_hdlc_setup_t;

void osp_ic_iec_hdlc_setup_init(osp_ic_iec_hdlc_setup_t *h, osp_obis_t ln);
const osp_ic_class_t *osp_ic_iec_hdlc_setup_class(void);
#endif
