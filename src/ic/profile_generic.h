#ifndef OSP_IC_PROFILE_GENERIC_H
#define OSP_IC_PROFILE_GENERIC_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_profile_buffer_t buffer;
	osp_capture_object_list_t capture_objects;
	uint32_t capture_period;
	osp_sort_method_t sort_method;
	osp_capture_object_t sort_object;
	uint32_t entries_in_use;
	uint32_t profile_entries;
} osp_ic_profile_generic_t;

void osp_ic_profile_generic_init(osp_ic_profile_generic_t *p, osp_obis_t ln);
const osp_ic_class_t *osp_ic_profile_generic_class(void);
#endif
