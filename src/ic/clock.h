#ifndef OSP_IC_CLOCK_H
#define OSP_IC_CLOCK_H
#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	osp_datetime_t time;
	uint32_t timezone_offset; /* minutes from UTC */
	uint8_t clock_status;     /* 0=ok, 1=error */
	uint32_t daylight_savings_begin;
	uint32_t daylight_savings_end;
	uint8_t daylight_savings_deviation;
	uint8_t daylight_savings_enabled;
} osp_ic_clock_t;

void osp_ic_clock_init(osp_ic_clock_t *c, osp_obis_t ln);
const osp_ic_class_t *osp_ic_clock_class(void);
#endif
