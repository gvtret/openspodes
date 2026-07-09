#ifndef OSP_IC_EXTENDED_REGISTER_H
#define OSP_IC_EXTENDED_REGISTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_value_t value;
	osp_scaler_unit_t scaler_unit;
	osp_value_t status;
	osp_obis_t capture_time;
} osp_ic_ext_register_t;

void osp_ic_ext_register_init(osp_ic_ext_register_t *r, osp_obis_t ln);
const osp_ic_class_t *osp_ic_ext_register_class(void);
#endif
