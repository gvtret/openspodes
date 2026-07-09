#ifndef OSP_IC_UTILITY_TABLES_H
#define OSP_IC_UTILITY_TABLES_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_utility_tables_t;

void osp_ic_utility_tables_init(osp_ic_utility_tables_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_utility_tables_class(void);
#endif
