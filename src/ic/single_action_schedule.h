#ifndef OSP_IC_SINGLE_ACTION_SCHEDULE_H
#define OSP_IC_SINGLE_ACTION_SCHEDULE_H
#include "../openspodes.h"

#ifndef OSP_MAX_EXECUTION_TIMES
#define OSP_MAX_EXECUTION_TIMES 8
#endif

/* execution_time_date ::= structure { time: octet-string(4), date: octet-string(5) } */
typedef struct {
	uint8_t time[4];
	uint8_t date[5];
} osp_execution_time_date_t;

typedef struct {
	osp_obis_t logical_name;
	osp_obis_t script_logical_name;
	uint16_t script_selector;
	uint8_t schedule_type;
	osp_execution_time_date_t execution_time[OSP_MAX_EXECUTION_TIMES];
	uint8_t execution_time_count;
} osp_ic_single_action_schedule_t;

void osp_ic_single_action_schedule_init(osp_ic_single_action_schedule_t *s, osp_obis_t ln);
const osp_ic_class_t *osp_ic_single_action_schedule_class(void);
#endif
