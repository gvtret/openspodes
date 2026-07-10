#ifndef OSP_IC_DISCONNECT_CONTROL_H
#define OSP_IC_DISCONNECT_CONTROL_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t output_state;
	uint8_t control_state;
	uint8_t control_model;
	osp_obis_t control_mode_script;
	osp_obis_t normal_position_script;
	osp_obis_t disconnect_position_script;
	uint32_t power_failure_counter;
} osp_ic_disconnect_control_t;

void osp_ic_disconnect_control_init(osp_ic_disconnect_control_t *d, osp_obis_t ln);
const osp_ic_class_t *osp_ic_disconnect_control_class(void);
#endif
