#ifndef OSP_IC_REGISTER_TABLE_H
#define OSP_IC_REGISTER_TABLE_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_register_table_t;

void osp_ic_register_table_init(osp_ic_register_table_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_register_table_class(void);
#endif
