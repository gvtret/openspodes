#ifndef OSP_IC_SCRIPT_TABLE_H
#define OSP_IC_SCRIPT_TABLE_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_script_t scripts[OSP_MAX_SCRIPTS];
	uint8_t script_count;
} osp_ic_script_table_t;

void osp_ic_script_table_init(osp_ic_script_table_t *t, osp_obis_t ln);
const osp_ic_class_t *osp_ic_script_table_class(void);
#endif
