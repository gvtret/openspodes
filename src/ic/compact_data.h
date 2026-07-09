#ifndef OSP_IC_COMPACT_DATA_H
#define OSP_IC_COMPACT_DATA_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_compact_data_t;

void osp_ic_compact_data_init(osp_ic_compact_data_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_compact_data_class(void);
#endif
