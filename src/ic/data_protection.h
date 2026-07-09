#ifndef OSP_IC_DATA_PROTECTION_H
#define OSP_IC_DATA_PROTECTION_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_data_protection_list_t protection_methods;
} osp_ic_data_protection_t;

void osp_ic_data_protection_init(osp_ic_data_protection_t *d, osp_obis_t ln);
const osp_ic_class_t *osp_ic_data_protection_class(void);
#endif
