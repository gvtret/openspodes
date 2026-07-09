#ifndef OSP_IC_IPV4_SETUP_H
#define OSP_IC_IPV4_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_obis_t dl_reference;
	uint32_t ip_address;
	uint32_t multicast_ip[OSP_MAX_IP_MULTICAST];
	uint8_t multicast_count;
	uint32_t subnet_mask;
	uint32_t gateway_ip;
	bool use_dhcp;
	uint32_t primary_dns;
	uint32_t secondary_dns;
} osp_ic_ipv4_setup_t;

void osp_ic_ipv4_setup_init(osp_ic_ipv4_setup_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_ipv4_setup_class(void);
#endif
