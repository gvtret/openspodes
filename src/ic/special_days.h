#ifndef OSP_IC_SPECIAL_DAYS_H
#define OSP_IC_SPECIAL_DAYS_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_special_day_t entries[32];
	uint8_t entry_count;
} osp_ic_special_days_t;

void osp_ic_special_days_init(osp_ic_special_days_t *d, osp_obis_t ln);
const osp_ic_class_t *osp_ic_special_days_class(void);
#endif
