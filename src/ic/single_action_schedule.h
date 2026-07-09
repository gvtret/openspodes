#ifndef OSP_IC_SINGLE_ACTION_SCHEDULE_H
#define OSP_IC_SINGLE_ACTION_SCHEDULE_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint32_t executed_script_id;
	uint8_t schedule_type;
	uint8_t execution_time[4];
} osp_ic_single_action_schedule_t;

void osp_ic_single_action_schedule_init(osp_ic_single_action_schedule_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_single_action_schedule_class(void);
#endif
