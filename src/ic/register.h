/**
 * Register interface class (class_id = 3, IEC 62056-6-2)
 */
#ifndef OSP_IC_REGISTER_H
#define OSP_IC_REGISTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_value_t value;
	osp_scaler_unit_t scaler_unit;
} osp_ic_register_t;

void osp_ic_register_init(osp_ic_register_t *r, osp_obis_t ln, osp_value_t val);
const osp_ic_class_t *osp_ic_register_class(void);
#endif
