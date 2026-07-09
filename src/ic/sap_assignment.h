#ifndef OSP_IC_SAP_ASSIGNMENT_H
#define OSP_IC_SAP_ASSIGNMENT_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_channel_list_t channel_list;
} osp_ic_sap_assignment_t;

void osp_ic_sap_assignment_init(osp_ic_sap_assignment_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_sap_assignment_class(void);
#endif
