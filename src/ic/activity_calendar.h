#ifndef OSP_IC_ACTIVITY_CALENDAR_H
#define OSP_IC_ACTIVITY_CALENDAR_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_season_t season_profile_active[OSP_MAX_SEASON_PROFILE];
	uint8_t season_count_active;
	osp_week_profile_t week_profile_table_active[OSP_MAX_WEEK_PROFILE];
	uint8_t week_count_active;
	osp_day_profile_t day_profile_table_active[OSP_MAX_DAY_PROFILE];
	uint8_t day_count_active;
	osp_season_t season_profile_passive[OSP_MAX_SEASON_PROFILE];
	uint8_t season_count_passive;
	osp_week_profile_t week_profile_table_passive[OSP_MAX_WEEK_PROFILE];
	uint8_t week_count_passive;
	osp_day_profile_t day_profile_table_passive[OSP_MAX_DAY_PROFILE];
	uint8_t day_count_passive;
} osp_ic_activity_calendar_t;

void osp_ic_activity_calendar_init(osp_ic_activity_calendar_t *a, osp_obis_t ln);
const osp_ic_class_t *osp_ic_activity_calendar_class(void);
#endif
