#ifndef OSP_IC_REGISTER_ACTIVATION_H
#define OSP_IC_REGISTER_ACTIVATION_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_register_list_t register_assignment;
	osp_mask_list_t mask_list;
	uint8_t active_mask[32];
	uint32_t active_mask_bits;
} osp_ic_register_activation_t;

void osp_ic_register_activation_init(osp_ic_register_activation_t *a, osp_obis_t ln);
const osp_ic_class_t *osp_ic_register_activation_class(void);
#endif
