#ifndef OSP_IC_DEMAND_REGISTER_H
#define OSP_IC_DEMAND_REGISTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_value_t current_average_value;
	osp_value_t last_average_value;
	osp_scaler_unit_t scaler_unit;
	osp_value_t status;
	osp_obis_t capture_time;
	osp_obis_t start_time_current;
	uint32_t period;
	uint16_t number_of_periods;
} osp_ic_demand_register_t;

void osp_ic_demand_register_init(osp_ic_demand_register_t *d, osp_obis_t ln);
const osp_ic_class_t *osp_ic_demand_register_class(void);
#endif
