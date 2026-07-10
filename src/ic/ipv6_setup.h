#ifndef OSP_IC_IPV6_SETUP_H
#define OSP_IC_IPV6_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_obis_t dl_reference;
	uint8_t address_config_mode;
	uint8_t traffic_class;
	uint8_t primary_dns[16];
	uint8_t primary_dns_len;
	uint8_t secondary_dns[16];
	uint8_t secondary_dns_len;
} osp_ic_ipv6_setup_t;

void osp_ic_ipv6_setup_init(osp_ic_ipv6_setup_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_ipv6_setup_class(void);
#endif
