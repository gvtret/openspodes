#ifndef OSP_IC_IEC_LOCAL_PORT_SETUP_H
#define OSP_IC_IEC_LOCAL_PORT_SETUP_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t default_mode;
	uint8_t default_baud_rate;
	uint8_t proposed_baud_rate;
	uint8_t response_time;
	uint8_t device_address[16];
	uint8_t device_address_len;
	uint8_t pass_p1[8];
	uint8_t pass_p1_len;
	uint8_t pass_p2[8];
	uint8_t pass_p2_len;
	uint8_t pass_w5;
} osp_ic_iec_local_port_setup_t;

void osp_ic_iec_local_port_setup_init(osp_ic_iec_local_port_setup_t *p, osp_obis_t ln);
const osp_ic_class_t *osp_ic_iec_local_port_setup_class(void);
#endif
