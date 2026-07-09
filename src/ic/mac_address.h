#ifndef OSP_IC_MAC_ADDRESS_H
#define OSP_IC_MAC_ADDRESS_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_mac_address_t;

void osp_ic_mac_address_init(osp_ic_mac_address_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_mac_address_class(void);
#endif
