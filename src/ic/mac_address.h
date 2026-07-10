#ifndef OSP_IC_MAC_ADDRESS_H
#define OSP_IC_MAC_ADDRESS_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t mac_address[6];
	uint8_t mac_address_len;
} osp_ic_mac_address_t;

void osp_ic_mac_address_init(osp_ic_mac_address_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_mac_address_class(void);

#endif
