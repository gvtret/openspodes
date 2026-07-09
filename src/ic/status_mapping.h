#ifndef OSP_IC_STATUS_MAPPING_H
#define OSP_IC_STATUS_MAPPING_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_status_mapping_t;

void osp_ic_status_mapping_init(osp_ic_status_mapping_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_status_mapping_class(void);
#endif
