#ifndef OSP_IC_SAP_ASSIGNMENT_H
#define OSP_IC_SAP_ASSIGNMENT_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	uint16_t sap;
	uint8_t logical_device_name[64];
	uint8_t logical_device_name_len;
} osp_sap_assignment_item_t;

typedef struct {
	osp_sap_assignment_item_t items[16];
	uint8_t count;
} osp_sap_assignment_list_t;

typedef struct {
	osp_obis_t logical_name;
	osp_sap_assignment_list_t sap_list;
} osp_ic_sap_assignment_t;

void osp_ic_sap_assignment_init(osp_ic_sap_assignment_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_sap_assignment_class(void);

#endif
