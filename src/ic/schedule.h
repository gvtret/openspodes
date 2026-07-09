#ifndef OSP_IC_SCHEDULE_H
#define OSP_IC_SCHEDULE_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_schedule_entry_t entries[OSP_MAX_SCHEDULE_ENTRY];
	uint8_t entry_count;
} osp_ic_schedule_t;

void osp_ic_schedule_init(osp_ic_schedule_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_schedule_class(void);
#endif
