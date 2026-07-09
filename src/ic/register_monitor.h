#ifndef OSP_IC_REGISTER_MONITOR_H
#define OSP_IC_REGISTER_MONITOR_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_value_definition_t monitored_value;
	osp_threshold_list_t thresholds;
} osp_ic_register_monitor_t;

void osp_ic_register_monitor_init(osp_ic_register_monitor_t *m, osp_obis_t ln);
const osp_ic_class_t *osp_ic_register_monitor_class(void);
#endif
