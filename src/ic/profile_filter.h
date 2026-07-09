#ifndef OSP_IC_PROFILE_FILTER_H
#define OSP_IC_PROFILE_FILTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_profile_filter_entry_t entries[OSP_MAX_FILTER_ENTRIES];
	uint8_t entry_count;
} osp_ic_profile_filter_t;

void osp_ic_profile_filter_init(osp_ic_profile_filter_t *f, osp_obis_t ln);
const osp_ic_class_t *osp_ic_profile_filter_class(void);
#endif
