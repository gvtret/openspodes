#ifndef OSP_IC_IPV6_SETUP_H
#define OSP_IC_IPV6_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_ipv6_setup_t;

void osp_ic_ipv6_setup_init(osp_ic_ipv6_setup_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_ipv6_setup_class(void);
#endif
