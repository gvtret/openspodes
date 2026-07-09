#ifndef OSP_IC_TCP_UDP_SETUP_H
#define OSP_IC_TCP_UDP_SETUP_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	osp_obis_t ip_setup_reference;
	uint16_t port;
	bool tcp_no_delay;
} osp_ic_tcp_udp_setup_t;

void osp_ic_tcp_udp_setup_init(osp_ic_tcp_udp_setup_t *t, osp_obis_t ln);
const osp_ic_class_t *osp_ic_tcp_udp_setup_class(void);
#endif
