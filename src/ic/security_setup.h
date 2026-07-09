#ifndef OSP_IC_SECURITY_SETUP_H
#define OSP_IC_SECURITY_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t security_policy;
	uint8_t security_suite;
	osp_octetstring_t client_system_title;
	osp_octetstring_t server_system_title;
} osp_ic_security_setup_t;

void osp_ic_security_setup_init(osp_ic_security_setup_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_security_setup_class(void);
#endif
