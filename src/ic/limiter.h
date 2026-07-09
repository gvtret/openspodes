#ifndef OSP_IC_LIMITER_H
#define OSP_IC_LIMITER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_value_definition_t monitored_value;
	osp_threshold_t threshold_active;
	osp_threshold_t threshold_normal;
	osp_threshold_t threshold_emergency;
	uint32_t min_over_threshold_duration;
	uint32_t min_under_threshold_duration;
	osp_emergency_profile_t emergency_profile;
	bool emergency_profile_active;
	osp_limiter_action_t actions;
} osp_ic_limiter_t;

void osp_ic_limiter_init(osp_ic_limiter_t *l, osp_obis_t ln);
const osp_ic_class_t *osp_ic_limiter_class(void);
#endif
